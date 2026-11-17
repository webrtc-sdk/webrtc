/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/fec_controller.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/environment/environment_factory.h"
#include "api/rtp_parameters.h"
#include "modules/include/module_fec_types.h"
#include "modules/video_coding/fec_controller_default.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

static const int kCodecBitrateBps = 100000;

class ProtectionBitrateCalculatorTest : public ::testing::Test {
 protected:
  enum {
    kSampleRate = 90000  // RTP timestamps per second.
  };

  class ProtectionCallback : public VCMProtectionCallback {
   public:
    int ProtectionRequest(const FecProtectionParams* delta_params,
                          const FecProtectionParams* key_params,
                          uint32_t* sent_video_rate_bps,
                          uint32_t* sent_nack_rate_bps,
                          uint32_t* sent_fec_rate_bps) override {
      delta_params_ = *delta_params;
      key_params_ = *key_params;
      *sent_video_rate_bps = kCodecBitrateBps;
      *sent_nack_rate_bps = nack_rate_bps_;
      *sent_fec_rate_bps = fec_rate_bps_;
      return 0;
    }
    void SetRetransmissionMode(int /* retransmission_mode */) override {}

    uint32_t fec_rate_bps_ = 0;
    uint32_t nack_rate_bps_ = 0;
    FecProtectionParams delta_params_;
    FecProtectionParams key_params_;
  };

  void UpdateFecRates(uint8_t fraction_lost, int64_t round_trip_time_ms) {
    fec_controller_.UpdateFecRates(kCodecBitrateBps, 30, fraction_lost,
                                   std::vector<bool>(1, false),
                                   round_trip_time_ms);
  }

  // Note: simulated clock starts at 1 seconds, since parts of webrtc use 0 as
  // a special case (e.g. frame rate in media optimization).
  ProtectionBitrateCalculatorTest()
      : clock_(1000),
        fec_controller_(CreateEnvironment(&clock_), &protection_callback_) {}

  SimulatedClock clock_;
  ProtectionCallback protection_callback_;
  FecControllerDefault fec_controller_;
};

TEST_F(ProtectionBitrateCalculatorTest, ProtectsUsingFecBitrate) {
  static const uint32_t kMaxBitrateBps = 130000;

  fec_controller_.SetProtectionMethod(true /*enable_fec*/,
                                      false /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  // Using 10% of codec bitrate for FEC.
  protection_callback_.fec_rate_bps_ = kCodecBitrateBps / 10;
  uint32_t target_bitrate = fec_controller_.UpdateFecRates(
      kMaxBitrateBps, 30, 0, std::vector<bool>(1, false), 0);

  EXPECT_GT(target_bitrate, 0u);
  EXPECT_GT(kMaxBitrateBps, target_bitrate);

  // Using as much for codec bitrate as fec rate, new target rate should share
  // both equally, but only be half of max (since that ceiling should be hit).
  protection_callback_.fec_rate_bps_ = kCodecBitrateBps;
  target_bitrate = fec_controller_.UpdateFecRates(
      kMaxBitrateBps, 30, 128, std::vector<bool>(1, false), 100);
  EXPECT_EQ(kMaxBitrateBps / 2, target_bitrate);
}

TEST_F(ProtectionBitrateCalculatorTest, ProtectsUsingNackBitrate) {
  static const uint32_t kMaxBitrateBps = 130000;

  fec_controller_.SetProtectionMethod(false /*enable_fec*/,
                                      true /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  uint32_t target_bitrate = fec_controller_.UpdateFecRates(
      kMaxBitrateBps, 30, 0, std::vector<bool>(1, false), 0);

  EXPECT_EQ(kMaxBitrateBps, target_bitrate);

  // Using as much for codec bitrate as nack rate, new target rate should share
  // both equally, but only be half of max (since that ceiling should be hit).
  protection_callback_.nack_rate_bps_ = kMaxBitrateBps;
  target_bitrate = fec_controller_.UpdateFecRates(
      kMaxBitrateBps, 30, 128, std::vector<bool>(1, false), 100);
  EXPECT_EQ(kMaxBitrateBps / 2, target_bitrate);
}

TEST_F(ProtectionBitrateCalculatorTest, NoProtection) {
  static const uint32_t kMaxBitrateBps = 130000;

  fec_controller_.SetProtectionMethod(false /*enable_fec*/,
                                      false /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  uint32_t target_bitrate = fec_controller_.UpdateFecRates(
      kMaxBitrateBps, 30, 128, std::vector<bool>(1, false), 100);
  EXPECT_EQ(kMaxBitrateBps, target_bitrate);
}

TEST_F(ProtectionBitrateCalculatorTest, FixedProtectionIgnoresLowRtt) {
  fec_controller_.SetProtectionMethod(true /*enable_fec*/,
                                      true /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  // With NACK and a low RTT, the adaptive logic turns delta frame FEC off.
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 0);

  RtpFecProtection protection;
  protection.mode = RtpFecProtection::Mode::kFixed;
  protection.protection_factor = 0.25;
  protection.key_frame_protection_factor = 0.5;
  fec_controller_.SetProtectionOverride(protection);
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 64);
  EXPECT_EQ(protection_callback_.key_params_.fec_rate, 128);
  EXPECT_EQ(protection_callback_.delta_params_.max_fec_frames, 1);
  EXPECT_EQ(protection_callback_.key_params_.max_fec_frames, 1);
  EXPECT_EQ(protection_callback_.delta_params_.fec_mask_type, kFecMaskRandom);

  protection.max_fec_frames = 3;
  fec_controller_.SetProtectionOverride(protection);
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.max_fec_frames, 3);
  EXPECT_EQ(protection_callback_.key_params_.max_fec_frames, 3);
}

TEST_F(ProtectionBitrateCalculatorTest, DisabledProtectionOverridesLoss) {
  fec_controller_.SetProtectionMethod(true /*enable_fec*/,
                                      false /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  UpdateFecRates(/*fraction_lost=*/64, /*round_trip_time_ms=*/100);
  EXPECT_GT(protection_callback_.delta_params_.fec_rate, 0);

  RtpFecProtection protection;
  protection.mode = RtpFecProtection::Mode::kDisabled;
  fec_controller_.SetProtectionOverride(protection);
  UpdateFecRates(/*fraction_lost=*/64, /*round_trip_time_ms=*/100);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 0);
  EXPECT_EQ(protection_callback_.key_params_.fec_rate, 0);
}

TEST_F(ProtectionBitrateCalculatorTest, MinimumProtectionKeepsHigherRate) {
  fec_controller_.SetProtectionMethod(true /*enable_fec*/,
                                      false /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  RtpFecProtection protection;
  protection.mode = RtpFecProtection::Mode::kMinimum;
  protection.protection_factor = 0.1;
  fec_controller_.SetProtectionOverride(protection);

  // Without loss the floor applies.
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/100);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 26);
  EXPECT_EQ(protection_callback_.key_params_.fec_rate, 26);

  // A higher adaptive rate is kept.
  UpdateFecRates(/*fraction_lost=*/64, /*round_trip_time_ms=*/100);
  EXPECT_GT(protection_callback_.delta_params_.fec_rate, 26);
}

TEST_F(ProtectionBitrateCalculatorTest, ResetProtectionRestoresAdaptive) {
  fec_controller_.SetProtectionMethod(true /*enable_fec*/,
                                      true /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  RtpFecProtection protection;
  protection.mode = RtpFecProtection::Mode::kFixed;
  protection.protection_factor = 0.5;
  fec_controller_.SetProtectionOverride(protection);
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 128);

  fec_controller_.SetProtectionOverride(std::nullopt);
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 0);
}

TEST_F(ProtectionBitrateCalculatorTest, ProtectionOverrideRequiresFec) {
  fec_controller_.SetProtectionMethod(false /*enable_fec*/,
                                      true /* enable_nack */);
  fec_controller_.SetEncodingData(640, 480, 1, 1000);

  RtpFecProtection protection;
  protection.mode = RtpFecProtection::Mode::kFixed;
  protection.protection_factor = 0.5;
  fec_controller_.SetProtectionOverride(protection);
  UpdateFecRates(/*fraction_lost=*/0, /*round_trip_time_ms=*/0);
  EXPECT_EQ(protection_callback_.delta_params_.fec_rate, 0);
  EXPECT_EQ(protection_callback_.key_params_.fec_rate, 0);
}

}  // namespace webrtc
