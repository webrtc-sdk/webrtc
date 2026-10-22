/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

#include <openssl/sha.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "api/crypto/frame_crypto_transformer.h"
#include "api/test/mock_transformable_video_frame.h"
#include "rtc_base/event.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// Byte-for-byte copies of livekit-server pkg/sfu/downtrack.go blank frames.
const std::vector<uint8_t> kVp8KeyFrame8x8 = {
    0x10, 0x02, 0x00, 0x9d, 0x01, 0x2a, 0x08, 0x00, 0x08, 0x00, 0x00,
    0x47, 0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x02, 0x02, 0x00,
    0x0c, 0x0d, 0x60, 0x00, 0xfe, 0xff, 0xab, 0x50, 0x80};
const std::vector<uint8_t> kH264Sps = {
    0x67, 0x42, 0xc0, 0x1f, 0x0f, 0xd9, 0x1f, 0x88, 0x88, 0x84, 0x00, 0x00,
    0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x3c, 0x60, 0xc9, 0x20};
const std::vector<uint8_t> kH264Pps = {0x68, 0x87, 0xcb, 0x83, 0xcb, 0x20};
const std::vector<uint8_t> kH264Idr = {0x65, 0x88, 0x84, 0x0a, 0xf2,
                                       0x62, 0x80, 0x00, 0xa7, 0xbe};

std::vector<uint8_t> OpusSilence() {
  std::vector<uint8_t> v(80, 0x00);
  v[0] = 0xf8;
  v[1] = 0xff;
  v[2] = 0xfe;
  return v;
}

std::vector<uint8_t> Concat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
  return out;
}

const std::vector<uint8_t> kStartCode4 = {0x00, 0x00, 0x00, 0x01};

std::string Sha256Hex(const std::vector<uint8_t>& data) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(data.data(), data.size(), digest);
  std::ostringstream s;
  for (uint8_t b : digest)
    s << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
  return s.str();
}

// The payload set must stay identical to client-sdk-js
// src/e2ee/worker/sifPayload.ts, which pins these SHA-256 digests.
TEST(SifPayload, MatchesJsSdkHashes) {
  EXPECT_EQ(Sha256Hex(kVp8KeyFrame8x8),
            "ef0161653d8b2b23aad46624b420af1d03ce48950e9fc85718028f91b50f9219");
  EXPECT_EQ(Sha256Hex(kH264Sps),
            "f0a0e09647d891d6d50aa898bce7108090375d0d55e50a2bb21147afee558e44");
  EXPECT_EQ(Sha256Hex(kH264Pps),
            "61d9665eed71b6d424ae9539330a3bdd5cb386d4d781c808219a6e36750493a7");
  EXPECT_EQ(Sha256Hex(kH264Idr),
            "faffc26b68a2fc09096fa20f3351e706398b6f838a7500c8063472c2e476e90d");
  EXPECT_EQ(Sha256Hex(OpusSilence()),
            "aad8d31fc56b2802ca500e58c2fb9d0b29ad71bb7cb52cd6530251eade188988");
}

TEST(SifPayload, AcceptsServerBlankFrames) {
  EXPECT_TRUE(IsKnownSifPayload(kVp8KeyFrame8x8));
  EXPECT_TRUE(IsKnownSifPayload(OpusSilence()));
  // opus/red primary block: header byte with F=0 then the silence frame.
  EXPECT_TRUE(IsKnownSifPayload(Concat({{0x6f}, OpusSilence()})));
  EXPECT_TRUE(IsKnownSifPayload(std::vector<uint8_t>(160, 0xff)));  // PCMU
  EXPECT_TRUE(IsKnownSifPayload(std::vector<uint8_t>(160, 0xd5)));  // PCMA
  EXPECT_TRUE(IsKnownSifPayload(Concat({kStartCode4, kH264Sps, kStartCode4,
                                        kH264Pps, kStartCode4, kH264Idr})));
  EXPECT_TRUE(IsKnownSifPayload(Concat({kStartCode4, kH264Idr})));
}

TEST(SifPayload, RejectsAnythingElse) {
  EXPECT_FALSE(IsKnownSifPayload({}));
  EXPECT_FALSE(IsKnownSifPayload(std::vector<uint8_t>{0x01, 0x02, 0x03}));
  std::vector<uint8_t> tampered = kVp8KeyFrame8x8;
  tampered[10] ^= 0x01;
  EXPECT_FALSE(IsKnownSifPayload(tampered));
  EXPECT_FALSE(IsKnownSifPayload(Concat({kVp8KeyFrame8x8, {0x00}})));
  auto truncated = OpusSilence();
  truncated.pop_back();
  EXPECT_FALSE(IsKnownSifPayload(truncated));
  EXPECT_FALSE(IsKnownSifPayload(Concat({{0xef}, OpusSilence()})));  // F=1
  EXPECT_FALSE(IsKnownSifPayload(std::vector<uint8_t>(159, 0xff)));
  EXPECT_FALSE(IsKnownSifPayload(std::vector<uint8_t>(160, 0xfe)));
  // A real slice smuggled in behind the blank parameter sets.
  EXPECT_FALSE(IsKnownSifPayload(
      Concat({kStartCode4, kH264Sps, kStartCode4, kH264Pps, kStartCode4,
              {0x65, 0xde, 0xad, 0xbe, 0xef}})));
  // Bare NALUs without start codes are not the Annex B frame we forward.
  EXPECT_FALSE(IsKnownSifPayload(kH264Idr));
}

// End-to-end through the transformer: what a receiver does with a frame that
// carries the room's SIF trailer, with no key (CLTCU-4's scenario) and with one.
class Sink : public TransformedFrameCallback {
 public:
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override {
    ++count;
    event.Set();
  }
  void StartShortCircuiting() override {}
  int count = 0;
  Event event;
};

class Observer : public FrameCryptorTransformerObserver {
 public:
  void OnFrameCryptionStateChanged(const std::string participant_id,
                                   FrameCryptionState state) override {
    last = state;
    event.Set();
  }
  FrameCryptionState last = FrameCryptionState::kNew;
  Event event;
};

class SifTransformTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kSsrc = 1234;
  const std::vector<uint8_t> trailer_ = {'s', 'e', 'c', 'r', 'e', 't'};

  void SetUp() override {
    KeyProviderOptions options;
    options.shared_key = true;
    key_provider_ = make_ref_counted<DefaultKeyProviderImpl>(options);
    key_provider_->SetSifTrailer(trailer_);
    signaling_thread_ = Thread::Create();
    signaling_thread_->Start();
    transformer_ = scoped_refptr<FrameCryptorTransformer>(
        new FrameCryptorTransformer(
            signaling_thread_.get(), "remote",
            FrameCryptorTransformer::MediaType::kVideoFrame,
            FrameCryptorTransformer::Algorithm::kAesGcm, key_provider_));
    transformer_->SetEnabled(true);
    sink_ = make_ref_counted<Sink>();
    observer_ = make_ref_counted<Observer>();
    transformer_->RegisterFrameCryptorTransformerObserver(observer_);
    scoped_refptr<FrameTransformerInterface> as_interface = transformer_;
    as_interface->RegisterTransformedFrameSinkCallback(sink_, kSsrc);
  }

  void TearDown() override {
    scoped_refptr<FrameTransformerInterface> as_interface = transformer_;
    as_interface->UnregisterTransformedFrameSinkCallback(kSsrc);
    transformer_->UnRegisterFrameCryptorTransformerObserver();
    transformer_ = nullptr;
    signaling_thread_->Stop();
  }

  // Feeds `payload + trailer` as an incoming video frame; returns the bytes the
  // transformer wrote back with SetData, if any.
  std::vector<uint8_t> Transform(std::vector<uint8_t> payload) {
    data_ = payload;
    data_.insert(data_.end(), trailer_.begin(), trailer_.end());
    auto frame = std::make_unique<NiceMock<MockTransformableVideoFrame>>();
    ON_CALL(*frame, GetDirection())
        .WillByDefault(Return(TransformableFrameInterface::Direction::kReceiver));
    ON_CALL(*frame, GetSsrc()).WillByDefault(Return(kSsrc));
    ON_CALL(*frame, GetData())
        .WillByDefault(Return(std::span<const uint8_t>(data_)));
    ON_CALL(*frame, header()).WillByDefault(ReturnRef(header_));
    ON_CALL(*frame, SetData(::testing::_))
        .WillByDefault([this](std::span<const uint8_t> data) {
          written_.assign(data.begin(), data.end());
        });
    scoped_refptr<FrameTransformerInterface> as_interface = transformer_;
    as_interface->Transform(std::move(frame));
    return written_;
  }

  scoped_refptr<DefaultKeyProviderImpl> key_provider_;
  std::unique_ptr<Thread> signaling_thread_;
  scoped_refptr<FrameCryptorTransformer> transformer_;
  scoped_refptr<Sink> sink_;
  scoped_refptr<Observer> observer_;
  RTPVideoHeader header_;
  std::vector<uint8_t> data_;
  std::vector<uint8_t> written_;
};

TEST_F(SifTransformTest, ForwardsServerBlankFrameWithoutKeyAndStripsTrailer) {
  Transform(kVp8KeyFrame8x8);
  ASSERT_TRUE(sink_->event.Wait(TimeDelta::Seconds(5)));
  EXPECT_EQ(sink_->count, 1);
  EXPECT_EQ(written_, kVp8KeyFrame8x8);
}

TEST_F(SifTransformTest, DropsForeignPayloadCarryingTrailerWithoutKey) {
  Transform({0xde, 0xad, 0xbe, 0xef, 0x00, 0x11, 0x22, 0x33});
  ASSERT_TRUE(observer_->event.Wait(TimeDelta::Seconds(5)));
  EXPECT_EQ(observer_->last, FrameCryptionState::kDecryptionFailed);
  EXPECT_FALSE(sink_->event.Wait(TimeDelta::Millis(200)));
  EXPECT_EQ(sink_->count, 0);
}

TEST_F(SifTransformTest, DropsForeignPayloadCarryingTrailerWithKey) {
  key_provider_->SetSharedKey(0, std::vector<uint8_t>(16, 0x42));
  Transform(std::vector<uint8_t>(500, 0x7a));
  ASSERT_TRUE(observer_->event.Wait(TimeDelta::Seconds(5)));
  EXPECT_EQ(observer_->last, FrameCryptionState::kDecryptionFailed);
  EXPECT_FALSE(sink_->event.Wait(TimeDelta::Millis(200)));
  EXPECT_EQ(sink_->count, 0);
}

}  // namespace
}  // namespace webrtc
