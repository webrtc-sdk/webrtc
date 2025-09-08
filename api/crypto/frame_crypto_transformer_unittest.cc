#include "api/crypto/frame_crypto_transformer.h"

#include <memory>
#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(FrameCryptor, KeyProvider) {
  auto key_options = KeyProviderOptions();
  RTC_LOG(LS_INFO) << "DataPacketCrypt shared_key default: "
                   << key_options.shared_key;
  EXPECT_EQ(key_options.shared_key, false);
  EXPECT_EQ(key_options.key_ring_size, DEFAULT_KEYRING_SIZE);

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
  EXPECT_EQ(key_options.key_ring_size, DEFAULT_KEYRING_SIZE);
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
  SleepMs(200);
  auto encrypted_data = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data.ok());
  SleepMs(200);  // ensure different timestamp for IV generation
  auto encrypted_data2 = data_packet_cryptor->Encrypt(
      participant_id, 0,
      std::vector<uint8_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  EXPECT_TRUE(encrypted_data2.ok());

  EXPECT_NE(encrypted_data.value()->iv, encrypted_data2.value()->iv);
}

}  // namespace webrtc
