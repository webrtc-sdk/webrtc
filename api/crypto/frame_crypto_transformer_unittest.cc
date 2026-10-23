#include "api/crypto/frame_crypto_transformer.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/test/mock_transformable_video_frame.h"
#include "common_video/h264/pps_parser.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void ExpectH264ClearPrefix(std::vector<uint8_t> data, size_t expected) {
  EXPECT_EQ(GetH264UnencryptedBytes(data), expected);
  if (expected <= data.size()) {
    // The decryptor must find the same boundary with an arbitrary ciphertext
    // suffix, including one that looks like an emulation prevention byte.
    for (uint8_t byte : {0x00, 0x03, 0xff}) {
      std::fill(data.begin() + expected, data.end(), byte);
      EXPECT_EQ(GetH264UnencryptedBytes(data), expected);
    }
    data.resize(expected);
    EXPECT_EQ(GetH264UnencryptedBytes(data), expected);
  }
}

class CapturingFrameCallback : public TransformedFrameCallback {
 public:
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override {
    const auto data = frame->GetData();
    data_.assign(data.begin(), data.end());
    ready_.Set();
  }

  std::optional<std::vector<uint8_t>> WaitForData() {
    if (!ready_.Wait(TimeDelta::Seconds(5))) {
      return std::nullopt;
    }
    return data_;
  }

 private:
  std::vector<uint8_t> data_;
  Event ready_;
};

std::optional<std::vector<uint8_t>> TransformH264Frame(
    std::vector<uint8_t> data,
    TransformableFrameInterface::Direction direction,
    scoped_refptr<KeyProvider> key_provider) {
  RTPVideoHeader header;
  header.codec = kVideoCodecH264;
  auto frame =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame, header).WillByDefault(testing::ReturnRef(header));
  ON_CALL(*frame, IsKeyFrame).WillByDefault(testing::Return(true));
  ON_CALL(*frame, GetDirection).WillByDefault(testing::Return(direction));
  ON_CALL(*frame, GetSsrc).WillByDefault(testing::Return(1234u));
  ON_CALL(*frame, GetTimestamp).WillByDefault(testing::Return(5678u));
  ON_CALL(*frame, GetData).WillByDefault([&data] {
    return std::span<const uint8_t>(data);
  });
  EXPECT_CALL(*frame, SetData)
      .WillOnce([&data](std::span<const uint8_t> output) {
        data.assign(output.begin(), output.end());
      });

  auto callback = make_ref_counted<CapturingFrameCallback>();
  scoped_refptr<FrameCryptorTransformer> cryptor(new FrameCryptorTransformer(
      nullptr, "participant_1", FrameCryptorTransformer::MediaType::kVideoFrame,
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider));
  cryptor->SetEnabled(true);
  scoped_refptr<FrameTransformerInterface> transformer = cryptor;
  transformer->RegisterTransformedFrameSinkCallback(callback, 1234u);
  transformer->Transform(std::move(frame));
  return callback->WaitForData();
}

void ExpectH264FrameRoundTrip(std::span<const uint8_t> slice,
                              size_t clear_nalu_bytes) {
  auto options = KeyProviderOptions();
  options.shared_key = true;
  options.ratchet_salt = {0, 1, 2, 3, 4, 5, 6, 7};
  auto key_provider = make_ref_counted<DefaultKeyProviderImpl>(options);
  ASSERT_TRUE(key_provider->SetSharedKey(
      0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));

  std::vector<uint8_t> original{0, 0, 0,    1,    0x67, 0x42, 0, 0x1f, 0, 0,
                                0, 1, 0x68, 0xce, 0x06, 0xe2, 0, 0,    0, 1};
  const size_t idr_offset = original.size();
  original.insert(original.end(), slice.begin(), slice.end());
  original.insert(original.end(), {0x12, 0x34, 0x56, 0x78, 0, 0, 3, 1});

  const auto encrypted = TransformH264Frame(
      original, TransformableFrameInterface::Direction::kSender, key_provider);
  ASSERT_TRUE(encrypted.has_value());
  ASSERT_GT(encrypted->size(), original.size());
  EXPECT_NE(*encrypted, original);

  const size_t clear_bytes = idr_offset + clear_nalu_bytes;
  EXPECT_EQ(GetH264UnencryptedBytes(*encrypted), clear_bytes);
  EXPECT_THAT(std::span<const uint8_t>(*encrypted).first(clear_bytes),
              testing::ElementsAreArray(
                  std::span<const uint8_t>(original).first(clear_bytes)));
  // The depacketizer parses this before the receiver's decryptor runs.
  const auto slice_header = PpsParser::ParseSliceHeader(
      std::span<const uint8_t>(*encrypted).subspan(idr_offset + 1));
  ASSERT_TRUE(slice_header.has_value());
  EXPECT_EQ(slice_header->pic_parameter_set_id, 0u);

  const auto decrypted = TransformH264Frame(
      *encrypted, TransformableFrameInterface::Direction::kReceiver,
      key_provider);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, original);
}

}  // namespace

TEST(FrameCryptor, H264QuickSyncEncryptedPpsAndRoundTrip) {
  const uint8_t slice[] = {0x65, 0x88, 0x80, 0x40, 0x87, 0x0a};
  ExpectH264FrameRoundTrip(slice, 3);
}

TEST(FrameCryptor, H264VideoToolboxEncryptedPpsAndRoundTrip) {
  const uint8_t slice[] = {0x65, 0xb8, 0x00, 0x40, 0x00, 0x84};
  ExpectH264FrameRoundTrip(slice, 2);
}

TEST(H264UnencryptedBytes, VideoToolboxWithParameterSets) {
  const std::vector<uint8_t> data{0, 0, 0,    1,    0x67, 0x42, 0x00, 0x1f, 0,
                                  0, 0, 1,    0x68, 0xce, 0x06, 0xe2, 0,    0,
                                  0, 1, 0x65, 0xb8, 0x00, 0x40, 0x00, 0x84};
  // first_mb_in_slice=0 (1 bit), slice_type=2 (3), PPS id=0 (1).
  ExpectH264ClearPrefix(data, 20u + 2);
}

TEST(H264UnencryptedBytes, QuickSyncIdrAndSlice) {
  for (uint8_t header : {0x65, 0x61}) {
    // first_mb_in_slice=0 (1 bit), slice_type=7 (7), PPS id=0 (1).
    ExpectH264ClearPrefix({0, 0, 0, 1, header, 0x88, 0x80, 0x40, 0x87, 0x0a},
                          4u + 3);
  }
}

TEST(H264UnencryptedBytes, EmulationPreventionBytes) {
  // RBSP: 00 00 01 00 00 00 22; first_mb_in_slice=8388607 (47 bits),
  // slice_type=7 (7), PPS id=0 (1). Both 03 bytes must remain clear.
  ExpectH264ClearPrefix(
      {0, 0, 0, 1, 0x65, 0, 0, 3, 1, 0, 0, 3, 0, 0x22, 0x40, 0x87}, 4u + 10);
}

TEST(H264UnencryptedBytes, LargeParameterSets) {
  std::vector<uint8_t> data{0, 0, 0, 1, 0x67};
  data.resize(260, 0x55);
  const std::vector<uint8_t> tail{0, 0, 0, 1,    0x68, 0xce, 0x06, 0xe2, 0,
                                  0, 0, 1, 0x65, 0x88, 0x80, 0x40, 0x87, 0x0a};
  data.insert(data.end(), tail.begin(), tail.end());
  ExpectH264ClearPrefix(data, 272u + 3);
}

TEST(H264UnencryptedBytes, NaluHeaderOnlyFallsBack) {
  ExpectH264ClearPrefix({0, 0, 0, 1, 0x65}, 4u + 2);
}

TEST(H264UnencryptedBytes, TruncatedGolombFallsBack) {
  // first_mb_in_slice=0 and slice_type=7 fill the byte; PPS id is missing.
  ExpectH264ClearPrefix({0, 0, 0, 1, 0x65, 0x88}, 4u + 2);
}

TEST(H264UnencryptedBytes, OversizedHeaderFallsBack) {
  // Three 31-bit ue(v) fields require more than the nine-byte NALU cap.
  EXPECT_EQ(GetH264UnencryptedBytes(std::vector<uint8_t>{
                0, 0, 0, 1, 0x65, 0, 1, 0, 0, 3, 0, 2, 0, 0, 3, 0, 4, 0, 0}),
            4u + 2);
}

TEST(H264UnencryptedBytes, EmptyNalusAndNoSlice) {
  EXPECT_EQ(GetH264UnencryptedBytes({}), 0u);
  EXPECT_EQ(GetH264UnencryptedBytes(
                std::vector<uint8_t>{0, 0, 0, 1, 0, 0, 0, 1, 0x67}),
            0u);
}

TEST(H264UnencryptedBytes, EscapeAfterPpsIdIsOutsideClearPrefix) {
  // first_mb_in_slice=0, slice_type=0, PPS id=65535 (35 bits total).
  // The clear prefix ends in 00 00; a following 03 must not extend it.
  ExpectH264ClearPrefix({0, 0, 0, 1, 0x65, 0xc0, 0, 0x20, 0, 0, 3, 0x40},
                        4u + 6);
}

TEST(FrameCryptor, KeyProvider) {
  auto key_options = KeyProviderOptions();
  RTC_LOG(LS_INFO) << "DataPacketCrypt shared_key default: "
                   << key_options.shared_key;
  EXPECT_EQ(key_options.shared_key, false);
  EXPECT_EQ(key_options.key_ring_size, static_cast<int>(DEFAULT_KEYRING_SIZE));

  key_options.ratchet_salt =
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(key_options.ratchet_salt.size(), 8u);

  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);
  EXPECT_NE(key_provider, nullptr);

  std::string participant_id = "participant_1";
  key_provider->SetKey(participant_id, 0,
                       std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                            11, 12, 13, 14, 15});
  auto key_handler = key_provider->GetKey(participant_id);
  EXPECT_NE(key_handler, nullptr);

  auto keyset = key_handler->GetKeySet(0);
  EXPECT_NE(keyset, nullptr);

  EXPECT_EQ(keyset->material,
            std::vector<uint8_t>(
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_EQ(keyset->encryption_key.size(), 16u);

  EXPECT_EQ(keyset->encryption_key,
            std::vector<uint8_t>({166, 88, 205, 82, 239, 186, 202, 223, 236,
                                  223, 224, 160, 220, 87, 78, 195}));

  key_handler->RatchetKey(0);
  auto new_keyset = key_handler->GetKeySet(0);
  EXPECT_NE(new_keyset, nullptr);
  EXPECT_NE(new_keyset->material, keyset->material);
  EXPECT_NE(new_keyset->encryption_key, keyset->encryption_key);
}

TEST(DataPacketCryptor, BasicTest) {
  auto key_options = KeyProviderOptions();
  key_options.ratchet_salt =
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});
  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);
  EXPECT_NE(key_provider, nullptr);

  std::string participant_id = "participant_1";
  key_provider->SetKey(participant_id, 0,
                       std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                            11, 12, 13, 14, 15});
  auto data_packet_cryptor = webrtc::make_ref_counted<DataPacketCryptor>(
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider);
  EXPECT_NE(data_packet_cryptor, nullptr);
  RTC_LOG(LS_INFO) << "DataPacketCrypt test";

  auto encrypted_data = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data.ok());
  EXPECT_EQ(encrypted_data.value()->data.size(), 16 + 16u);  // data + tag
  EXPECT_NE(encrypted_data.value()->data,
            std::vector<uint8_t>(
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  RTC_LOG(LS_INFO) << "DataPacketCrypt encrypted_data size: "
                   << encrypted_data.value()->data.size();
  RTC_LOG(LS_INFO) << "DataPacketCrypt encrypted_data iv size: "
                   << encrypted_data.value()->iv.size();
  RTC_LOG(LS_INFO) << "DataPacketCrypt encrypted_data key_index: "
                   << static_cast<int>(encrypted_data.value()->key_index);
  EXPECT_EQ(encrypted_data.value()->key_index, 0);

  EXPECT_EQ(encrypted_data.value()->iv.size(), 12u);

  auto decrypted_data =
      data_packet_cryptor->Decrypt(participant_id, encrypted_data.value());

  EXPECT_TRUE(decrypted_data.ok());

  EXPECT_EQ(decrypted_data.value(),
            std::vector<uint8_t>(
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));

  auto key_handler = key_provider->GetKey(participant_id);
  EXPECT_NE(key_handler, nullptr);

  key_handler->RatchetKey(0);
  // decrypt with ratcheted key should fail
  auto decrypted_data2 =
      data_packet_cryptor->Decrypt(participant_id, encrypted_data.value());
  EXPECT_FALSE(decrypted_data2.ok());

  // set back to previous key
  key_provider->SetKey(participant_id, 0,
                       std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                            11, 12, 13, 14, 15});
  auto decrypted_data3 =
      data_packet_cryptor->Decrypt(participant_id, encrypted_data.value());
  EXPECT_TRUE(decrypted_data3.ok());
  EXPECT_EQ(decrypted_data3.value(),
            std::vector<uint8_t>(
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
}

TEST(DataPacketCryptor, DifferentKeyProvider) {
  auto key_options = KeyProviderOptions();
  RTC_LOG(LS_INFO) << "DataPacketCrypt shared_key default: "
                   << key_options.shared_key;
  EXPECT_EQ(key_options.shared_key, false);
  EXPECT_EQ(key_options.key_ring_size, static_cast<int>(DEFAULT_KEYRING_SIZE));
  // support ratcheting
  key_options.ratchet_window_size = 4;
  key_options.ratchet_salt =
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(key_options.ratchet_salt.size(), 8u);

  auto key_provider1 =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);
  EXPECT_NE(key_provider1, nullptr);

  auto key_provider2 =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);
  EXPECT_NE(key_provider2, nullptr);

  std::string participant_id = "participant_1";
  key_provider1->SetKey(participant_id, 0,
                        std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                             11, 12, 13, 14, 15});
  key_provider2->SetKey(participant_id, 0,
                        std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                             11, 12, 13, 14, 15});

  auto data_packet_cryptor1 = webrtc::make_ref_counted<DataPacketCryptor>(
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider1);
  EXPECT_NE(data_packet_cryptor1, nullptr);

  auto data_packet_cryptor2 = webrtc::make_ref_counted<DataPacketCryptor>(
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider2);
  EXPECT_NE(data_packet_cryptor2, nullptr);

  auto encrypted_data = data_packet_cryptor1->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));

  EXPECT_TRUE(encrypted_data.ok());
  EXPECT_EQ(encrypted_data.value()->data.size(), 16 + 16u);  // data + tag

  auto decrypted_data =
      data_packet_cryptor2->Decrypt(participant_id, encrypted_data.value());
  EXPECT_TRUE(decrypted_data.ok());

  key_provider1->RatchetKey(participant_id, 0);
  auto encrypted_data2 = data_packet_cryptor1->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data2.ok());

  // decrypt with auto-ratcheted key should be successful
  auto decrypted_data2 =
      data_packet_cryptor2->Decrypt(participant_id, encrypted_data2.value());
  EXPECT_TRUE(decrypted_data2.ok());
}

TEST(DataPacketCryptor, IVGeneration) {
  auto key_options = KeyProviderOptions();
  key_options.ratchet_salt =
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});
  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);

  std::string participant_id = "participant_1";
  key_provider->SetKey(participant_id, 0,
                       std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                            11, 12, 13, 14, 15});
  auto data_packet_cryptor = webrtc::make_ref_counted<DataPacketCryptor>(
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider);
  EXPECT_NE(data_packet_cryptor, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  auto encrypted_data = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data.ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(200));  // ensure different timestamp for IV generation
  auto encrypted_data2 = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data2.ok());

  EXPECT_NE(encrypted_data.value()->iv, encrypted_data2.value()->iv);
}

TEST(KeyProvider, KeyDerivationAlgorithm) {
    auto key_options = KeyProviderOptions();
  key_options.ratchet_salt =
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});
  key_options.key_derivation_algorithm = KeyDerivationAlgorithm::kHKDF;
  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);

  std::string participant_id = "participant_1";
  key_provider->SetKey(participant_id, 0,
                       std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                            11, 12, 13, 14, 15});
  auto data_packet_cryptor = webrtc::make_ref_counted<DataPacketCryptor>(
      FrameCryptorTransformer::Algorithm::kAesGcm, key_provider);
  EXPECT_NE(data_packet_cryptor, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  auto encrypted_data = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data.ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(200));  // ensure different timestamp for IV generation
  auto encrypted_data2 = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data2.ok());

  EXPECT_NE(encrypted_data.value()->iv, encrypted_data2.value()->iv);
}

TEST(KeyProvider, KeyRingSizeMaxAcceptsIndex255) {
  auto key_options = KeyProviderOptions();
  key_options.shared_key = true;
  key_options.key_ring_size = static_cast<int>(MAX_KEYRING_SIZE);
  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);

  const std::vector<uint8_t> key{0xAA, 0xBB, 0xCC, 0xDD};
  EXPECT_TRUE(key_provider->SetSharedKey(255, key));
  EXPECT_EQ(key_provider->ExportSharedKey(255), key);
}

TEST(KeyProvider, GetKeySetReturnsNullptrForOutOfRange) {
  auto key_options = KeyProviderOptions();
  key_options.shared_key = true;
  auto key_provider =
      webrtc::make_ref_counted<DefaultKeyProviderImpl>(key_options);
  ASSERT_TRUE(key_provider->SetSharedKey(0, std::vector<uint8_t>{0x00}));

  auto key_handler = key_provider->GetSharedKey("participant_1");
  ASSERT_NE(key_handler, nullptr);

  // -1 is the "use current_key_index_" sentinel — still resolves.
  EXPECT_NE(key_handler->GetKeySet(-1), nullptr);
  // DEFAULT_KEYRING_SIZE (16) → indices >= 16 are out of range.
  EXPECT_EQ(key_handler->GetKeySet(static_cast<int>(DEFAULT_KEYRING_SIZE)),
            nullptr);
  EXPECT_EQ(key_handler->GetKeySet(1000), nullptr);
  // Negative indices other than the -1 sentinel are out of range.
  EXPECT_EQ(key_handler->GetKeySet(-2), nullptr);
}

}  // namespace webrtc
