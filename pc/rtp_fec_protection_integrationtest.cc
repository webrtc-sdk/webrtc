/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// End-to-end tests for RtpParameters::fec_protection: two PeerConnections
// negotiate FlexFEC, and the FEC actually sent is observed through the
// outbound-rtp and inbound-rtp stats.

#include <cstdint>
#include <optional>

#include "absl/functional/any_invocable.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "pc/test/integration_test_helpers.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::Gt;
using ::testing::IsTrue;

constexpr char kFlexFecFieldTrials[] =
    "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";

struct SendCounters {
  uint64_t packets_sent = 0;
  uint64_t fec_packets_sent = 0;
  std::optional<uint32_t> fec_ssrc;
};

class RtpFecProtectionIntegrationTest
    : public PeerConnectionIntegrationBaseTest {
 public:
  RtpFecProtectionIntegrationTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}

 protected:
  // Sets up a one-way video call from the caller with FlexFEC negotiated, and
  // returns the caller's video sender. `before_offer` runs after the track is
  // added but before negotiation.
  scoped_refptr<RtpSenderInterface> StartVideoCall(
      absl::AnyInvocable<void(RtpSenderInterface&)> before_offer = nullptr) {
    SetFieldTrials(kFlexFecFieldTrials);
    if (!CreatePeerConnectionWrappers()) {
      ADD_FAILURE() << "Failed to create PeerConnections";
      return nullptr;
    }
    ConnectFakeSignaling();
    scoped_refptr<RtpSenderInterface> sender = caller()->AddVideoTrack();
    if (before_offer) {
      before_offer(*sender);
    }
    caller()->CreateAndSetAndSignalOffer();
    EXPECT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
                IsRtcOk());
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeVideo();
    EXPECT_TRUE(ExpectNewFrames(media_expectations));
    return sender;
  }

  // The fixture sends a single video stream; fails the test if the caller
  // reports any other number of video outbound-rtp stats.
  SendCounters GetCallerVideoCounters() {
    SendCounters counters;
    scoped_refptr<const RTCStatsReport> report =
        caller()->NewGetStats(run_loop());
    if (!report) {
      return counters;
    }
    int num_video_streams = 0;
    for (const RTCOutboundRtpStreamStats* outbound :
         report->GetStatsOfType<RTCOutboundRtpStreamStats>()) {
      if (outbound->kind != "video") {
        continue;
      }
      ++num_video_streams;
      counters.packets_sent = outbound->packets_sent.value_or(0);
      counters.fec_packets_sent = outbound->fec_packets_sent.value_or(0);
      counters.fec_ssrc = outbound->fec_ssrc;
    }
    EXPECT_EQ(num_video_streams, 1);
    return counters;
  }

  // Same single-stream assumption as `GetCallerVideoCounters`, for the
  // callee's video inbound-rtp.
  uint64_t GetCalleeFecPacketsReceived() {
    scoped_refptr<const RTCStatsReport> report =
        callee()->NewGetStats(run_loop());
    if (!report) {
      return 0;
    }
    int num_video_streams = 0;
    uint64_t fec_packets_received = 0;
    for (const RTCInboundRtpStreamStats* inbound :
         report->GetStatsOfType<RTCInboundRtpStreamStats>()) {
      if (inbound->kind != "video") {
        continue;
      }
      ++num_video_streams;
      fec_packets_received = inbound->fec_packets_received.value_or(0);
    }
    EXPECT_EQ(num_video_streams, 1);
    return fec_packets_received;
  }

  // Lets `settle` pass so the new policy is applied by the FEC controller and
  // in-flight FEC is accounted for, then returns how many FEC and total packets
  // were sent over `window`.
  SendCounters MeasureDelta(TimeDelta settle, TimeDelta window) {
    run_loop().RunFor(settle);
    SendCounters before = GetCallerVideoCounters();
    run_loop().RunFor(window);
    SendCounters after = GetCallerVideoCounters();
    SendCounters delta;
    delta.packets_sent = after.packets_sent - before.packets_sent;
    delta.fec_packets_sent = after.fec_packets_sent - before.fec_packets_sent;
    delta.fec_ssrc = after.fec_ssrc;
    return delta;
  }

  static void SetFecProtection(RtpSenderInterface& sender,
                               std::optional<RtpFecProtection> protection) {
    RtpParameters parameters = sender.GetParameters();
    parameters.fec_protection = protection;
    EXPECT_THAT(sender.SetParameters(parameters), IsRtcOk());
  }

  static RtpFecProtection Fixed(double factor,
                                std::optional<int> max_fec_frames = {}) {
    RtpFecProtection protection;
    protection.mode = RtpFecProtection::Mode::kFixed;
    protection.protection_factor = factor;
    protection.max_fec_frames = max_fec_frames;
    return protection;
  }
};

// With no loss, the default adaptive policy does not send any FEC even though
// FlexFEC is negotiated.
TEST_F(RtpFecProtectionIntegrationTest, AdaptiveSendsNoFecWithoutLoss) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);
  EXPECT_FALSE(sender->GetParameters().fec_protection.has_value());

  SendCounters delta =
      MeasureDelta(TimeDelta::Seconds(1), TimeDelta::Seconds(2));
  EXPECT_GT(delta.packets_sent, 0u);
  EXPECT_EQ(delta.fec_packets_sent, 0u);
}

// kFixed sends FEC even without loss, the receiver gets it, and kDisabled
// stops it again without renegotiation.
TEST_F(RtpFecProtectionIntegrationTest, FixedSendsFecAndDisabledStopsIt) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);

  SetFecProtection(*sender, Fixed(0.5));
  EXPECT_EQ(sender->GetParameters().fec_protection, Fixed(0.5));
  EXPECT_THAT(
      WaitUntil([&] { return GetCallerVideoCounters().fec_packets_sent; },
                Gt(0u)),
      IsRtcOk());
  EXPECT_THAT(WaitUntil([&] { return GetCalleeFecPacketsReceived(); }, Gt(0u)),
              IsRtcOk());
  // The FEC stats are reported on the protected stream once FEC was sent.
  EXPECT_TRUE(GetCallerVideoCounters().fec_ssrc.has_value());

  RtpFecProtection disabled;
  disabled.mode = RtpFecProtection::Mode::kDisabled;
  SetFecProtection(*sender, disabled);
  SendCounters delta =
      MeasureDelta(TimeDelta::Seconds(1), TimeDelta::Seconds(2));
  EXPECT_GT(delta.packets_sent, 0u);
  EXPECT_EQ(delta.fec_packets_sent, 0u);

  // Video keeps flowing after the changes.
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

// kMinimum keeps a FEC floor even when the adaptive logic would turn FEC off.
TEST_F(RtpFecProtectionIntegrationTest, MinimumSendsFecWithoutLoss) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);

  RtpFecProtection minimum;
  minimum.mode = RtpFecProtection::Mode::kMinimum;
  minimum.protection_factor = 0.3;
  SetFecProtection(*sender, minimum);
  SendCounters delta =
      MeasureDelta(TimeDelta::Seconds(1), TimeDelta::Seconds(2));
  EXPECT_GT(delta.fec_packets_sent, 0u);
}

// A higher fixed protection factor results in more FEC per media packet.
TEST_F(RtpFecProtectionIntegrationTest, HigherFixedFactorSendsMoreFec) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);

  // Protect several frames per FEC block so that the factor, not the minimum of
  // one FEC packet per block, determines the FEC count.
  SetFecProtection(*sender, Fixed(0.1, RtpFecProtection::kMaxFecFrames));
  SendCounters low = MeasureDelta(TimeDelta::Seconds(1), TimeDelta::Seconds(3));
  SetFecProtection(*sender, Fixed(0.9, RtpFecProtection::kMaxFecFrames));
  SendCounters high =
      MeasureDelta(TimeDelta::Seconds(1), TimeDelta::Seconds(3));

  ASSERT_GT(low.packets_sent, 0u);
  ASSERT_GT(high.packets_sent, 0u);
  double low_ratio =
      static_cast<double>(low.fec_packets_sent) / low.packets_sent;
  double high_ratio =
      static_cast<double>(high.fec_packets_sent) / high.packets_sent;
  RTC_LOG(LS_INFO) << "FEC ratio at 0.1: " << low_ratio << " ("
                   << low.fec_packets_sent << "/" << low.packets_sent
                   << "), at 0.9: " << high_ratio << " ("
                   << high.fec_packets_sent << "/" << high.packets_sent << ")";
  EXPECT_GT(high.fec_packets_sent, 0u);
  // Require a clear margin rather than any difference, so the test fails if
  // the factor has no real effect instead of passing on noise.
  EXPECT_GT(high_ratio, 2 * low_ratio);
}

// A policy set before negotiation (stored as init parameters while the sender
// has no SSRC) is applied once the stream is created.
TEST_F(RtpFecProtectionIntegrationTest, PolicySetBeforeNegotiationIsApplied) {
  scoped_refptr<RtpSenderInterface> sender =
      StartVideoCall([](RtpSenderInterface& sender) {
        SetFecProtection(sender, Fixed(0.5));
      });
  ASSERT_TRUE(sender);
  EXPECT_EQ(sender->GetParameters().fec_protection, Fixed(0.5));
  EXPECT_THAT(
      WaitUntil([&] { return GetCallerVideoCounters().fec_packets_sent; },
                Gt(0u)),
      IsRtcOk());
}

// Clearing the policy round-trips through getParameters().
TEST_F(RtpFecProtectionIntegrationTest, ClearingPolicyRestoresUnset) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);

  SetFecProtection(*sender, Fixed(0.5));
  ASSERT_TRUE(sender->GetParameters().fec_protection.has_value());
  SetFecProtection(*sender, std::nullopt);
  EXPECT_FALSE(sender->GetParameters().fec_protection.has_value());
}

TEST_F(RtpFecProtectionIntegrationTest, InvalidValuesAreRejected) {
  scoped_refptr<RtpSenderInterface> sender = StartVideoCall();
  ASSERT_TRUE(sender);

  RtpParameters parameters = sender->GetParameters();
  parameters.fec_protection = Fixed(1.5);
  EXPECT_EQ(sender->SetParameters(parameters).type(),
            RTCErrorType::INVALID_RANGE);

  parameters = sender->GetParameters();
  parameters.fec_protection = Fixed(0.5);
  parameters.fec_protection->key_frame_protection_factor = -0.1;
  EXPECT_EQ(sender->SetParameters(parameters).type(),
            RTCErrorType::INVALID_RANGE);

  parameters = sender->GetParameters();
  parameters.fec_protection =
      Fixed(0.5, RtpFecProtection::kMaxFecFrames + 1);
  EXPECT_EQ(sender->SetParameters(parameters).type(),
            RTCErrorType::INVALID_RANGE);

  parameters = sender->GetParameters();
  parameters.fec_protection = Fixed(0.5, 0);
  EXPECT_EQ(sender->SetParameters(parameters).type(),
            RTCErrorType::INVALID_RANGE);

  // Rejected values leave the previous (unset) policy in place.
  EXPECT_FALSE(sender->GetParameters().fec_protection.has_value());
}

// Rejected in RtpSenderBase::ValidateAndMaybeUpdateInitParameters, before FEC
// negotiation matters, so no FlexFEC field trials are needed.
TEST_F(RtpFecProtectionIntegrationTest, RejectedForAudioSender) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  scoped_refptr<RtpSenderInterface> sender = caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());

  RtpParameters parameters = sender->GetParameters();
  parameters.fec_protection = Fixed(0.5);
  EXPECT_EQ(sender->SetParameters(parameters).type(),
            RTCErrorType::INVALID_PARAMETER);
}

}  // namespace
}  // namespace webrtc
