/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

#include <cstdint>
#include <optional>
#include <vector>

#include "api/crypto/frame_crypto_transformer.h"
#include "api/test/mock_transformable_video_frame.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "rtc_base/event.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

const std::vector<uint8_t> kStartCode4 = {0x00, 0x00, 0x00, 0x01};
const std::vector<uint8_t> kSps = {
    0x67, 0x42, 0xc0, 0x1f, 0x0f, 0xd9, 0x1f, 0x88, 0x88, 0x84, 0x00, 0x00,
    0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x3c, 0x60, 0xc9, 0x20};
// pic_parameter_set_id 0.
const std::vector<uint8_t> kPps = {0x68, 0x87, 0xcb, 0x83, 0xcb, 0x20};
// Intel Quick Sync: slice_type 7 takes 7 bits, so the PPS id is the first bit
// of the third byte, the first encrypted bit.
const std::vector<uint8_t> kQsvIdr = {0x65, 0x88, 0x80, 0x40,
                                      0x87, 0x0a, 0x55, 0x66};
// VideoToolbox: slice_type 2 takes 3 bits, so the PPS id sits in the second
// byte.
const std::vector<uint8_t> kVtIdr = {0x25, 0xb8, 0x00, 0x40, 0x00, 0x84};

std::vector<uint8_t> Concat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto& p : parts)
    out.insert(out.end(), p.begin(), p.end());
  return out;
}

// Same vectors as client-sdk-js naluUtils.test.ts.
TEST(ParseH264IdrPpsId, ReadsThePpsIdPastTheSecondByteOfTheSlice) {
  EXPECT_EQ(ParseH264IdrPpsId(kQsvIdr), 0u);
  EXPECT_EQ(ParseH264IdrPpsId(kVtIdr), 0u);
  // first_mb_in_slice 0, slice_type 7, pic_parameter_set_id 1
  // (bits 1 0001000 010).
  EXPECT_EQ(ParseH264IdrPpsId(std::vector<uint8_t>{0x65, 0x88, 0x5f, 0xff}),
            1u);
}

TEST(ParseH264IdrPpsId, RejectsNonIdrSliceOrSliceEndingInsideTheField) {
  EXPECT_EQ(ParseH264IdrPpsId(std::vector<uint8_t>{0x41, 0x9a, 0x21, 0x43}),
            std::nullopt);
  EXPECT_EQ(ParseH264IdrPpsId(std::vector<uint8_t>{0x65, 0x88}), std::nullopt);
  EXPECT_EQ(ParseH264IdrPpsId(std::vector<uint8_t>{0x65}), std::nullopt);
}

class Sink : public TransformedFrameCallback {
 public:
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override {
    event.Set();
  }
  void StartShortCircuiting() override {}
  Event event;
};

// Encrypts H.264 frames through a sender transformer and decrypts them through
// a receiver transformer that share a key.
class H264PpsIdTransformTest : public ::testing::Test {
 protected:
  // The sender escapes the IV along with the ciphertext, but the receiver reads
  // the IV before unescaping, so keep zero bytes out of the IV here.
  static constexpr uint32_t kSsrc = 0x12345678;

  void SetUp() override {
    KeyProviderOptions options;
    options.shared_key = true;
    key_provider_ = make_ref_counted<DefaultKeyProviderImpl>(options);
    key_provider_->SetSharedKey(0, std::vector<uint8_t>(16, 0x42));
    signaling_thread_ = Thread::Create();
    signaling_thread_->Start();
    header_.codec = kVideoCodecH264;
    sender_ = CreateTransformer(sender_sink_);
    receiver_ = CreateTransformer(receiver_sink_);
  }

  void TearDown() override {
    for (auto& transformer : {sender_, receiver_}) {
      scoped_refptr<FrameTransformerInterface> as_interface = transformer;
      as_interface->UnregisterTransformedFrameSinkCallback(kSsrc);
    }
    sender_ = nullptr;
    receiver_ = nullptr;
    signaling_thread_->Stop();
  }

  scoped_refptr<FrameCryptorTransformer> CreateTransformer(
      scoped_refptr<Sink>& sink) {
    auto transformer =
        scoped_refptr<FrameCryptorTransformer>(new FrameCryptorTransformer(
            signaling_thread_.get(), "participant",
            FrameCryptorTransformer::MediaType::kVideoFrame,
            FrameCryptorTransformer::Algorithm::kAesGcm, key_provider_));
    transformer->SetEnabled(true);
    sink = make_ref_counted<Sink>();
    scoped_refptr<FrameTransformerInterface> as_interface = transformer;
    as_interface->RegisterTransformedFrameSinkCallback(sink, kSsrc);
    return transformer;
  }

  // Returns the bytes `transformer` wrote back with SetData.
  std::vector<uint8_t> Transform(
      scoped_refptr<FrameCryptorTransformer> transformer,
      Sink& sink,
      TransformableFrameInterface::Direction direction,
      const std::vector<uint8_t>& data) {
    std::vector<uint8_t> written;
    auto frame = std::make_unique<NiceMock<MockTransformableVideoFrame>>();
    ON_CALL(*frame, GetDirection()).WillByDefault(Return(direction));
    ON_CALL(*frame, GetSsrc()).WillByDefault(Return(kSsrc));
    ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(timestamp_));
    ON_CALL(*frame, GetData())
        .WillByDefault(Return(std::span<const uint8_t>(data)));
    ON_CALL(*frame, header()).WillByDefault(ReturnRef(header_));
    ON_CALL(*frame, SetData(::testing::_))
        .WillByDefault([&written](std::span<const uint8_t> out) {
          written.assign(out.begin(), out.end());
        });
    scoped_refptr<FrameTransformerInterface> as_interface = transformer;
    as_interface->Transform(std::move(frame));
    EXPECT_TRUE(sink.event.Wait(TimeDelta::Seconds(5)));
    return written;
  }

  std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data) {
    return Transform(sender_, *sender_sink_,
                     TransformableFrameInterface::Direction::kSender, data);
  }

  std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data) {
    return Transform(receiver_, *receiver_sink_,
                     TransformableFrameInterface::Direction::kReceiver, data);
  }

  scoped_refptr<DefaultKeyProviderImpl> key_provider_;
  std::unique_ptr<Thread> signaling_thread_;
  scoped_refptr<FrameCryptorTransformer> sender_;
  scoped_refptr<FrameCryptorTransformer> receiver_;
  scoped_refptr<Sink> sender_sink_;
  scoped_refptr<Sink> receiver_sink_;
  RTPVideoHeader header_;
  uint32_t timestamp_ = 0x23456789;
};

// Reads the PPS id from the last NALU of `frame` the way
// VideoRtpDepacketizerH264 does: the whole slice after the NALU header.
std::optional<uint32_t> ReceiverPpsId(const std::vector<uint8_t>& frame) {
  std::vector<H264::NaluIndex> nalus = H264::FindNaluIndices(frame);
  if (nalus.empty()) {
    return std::nullopt;
  }
  std::span<const uint8_t> slice = std::span<const uint8_t>(frame).subspan(
      nalus.back().payload_start_offset, nalus.back().payload_size);
  std::optional<PpsParser::SliceHeader> header =
      PpsParser::ParseSliceHeader(slice.subspan(H264::kNaluTypeSize));
  if (!header) {
    return std::nullopt;
  }
  return header->pic_parameter_set_id;
}

// A single encryption leaves a Quick Sync PPS id at 0 only half the time, so
// 32 keyframes in a row all pass without the IV retry with probability 2^-32.
TEST_F(H264PpsIdTransformTest, KeepsQuickSyncPpsIdReadable) {
  std::vector<uint8_t> idr = kQsvIdr;
  for (int i = 0; i < 500; i++)
    idr.push_back(static_cast<uint8_t>(i * 7 + 1));
  const std::vector<uint8_t> plain =
      Concat({kStartCode4, kSps, kStartCode4, kPps, kStartCode4, idr});

  for (int i = 0; i < 32; i++, timestamp_ += 3000) {
    std::vector<uint8_t> encrypted = Encrypt(plain);
    ASSERT_GT(encrypted.size(), plain.size());
    EXPECT_EQ(ReceiverPpsId(encrypted), 0u) << "keyframe " << i;
    EXPECT_EQ(Decrypt(encrypted), plain) << "keyframe " << i;
  }
}

TEST_F(H264PpsIdTransformTest, KeepsVideoToolboxPpsIdReadable) {
  const std::vector<uint8_t> plain =
      Concat({kStartCode4, kSps, kStartCode4, kPps, kStartCode4, kVtIdr});
  for (int i = 0; i < 8; i++, timestamp_ += 3000) {
    std::vector<uint8_t> encrypted = Encrypt(plain);
    EXPECT_EQ(ReceiverPpsId(encrypted), 0u);
    EXPECT_EQ(Decrypt(encrypted), plain);
  }
}

TEST_F(H264PpsIdTransformTest, RoundTripsDeltaFrame) {
  const std::vector<uint8_t> plain =
      Concat({kStartCode4, {0x41, 0x9a, 0x21, 0x43, 0x00, 0x00, 0x00, 0x7f}});
  std::vector<uint8_t> encrypted = Encrypt(plain);
  ASSERT_GT(encrypted.size(), plain.size());
  EXPECT_EQ(Decrypt(encrypted), plain);
}

}  // namespace
}  // namespace webrtc
