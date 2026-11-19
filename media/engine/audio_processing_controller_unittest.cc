/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/audio_processing_controller.h"

#include <optional>

#include "api/audio/audio_device.h"
#include "api/audio_options.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

// A coupled AEC/NS device module that counts live state readbacks. The base
// mock leaves the platform processing methods at their defaults, and the
// readback is what matters here: on Apple it queries the running engine.
class CoupledPlatformAudioDeviceModule
    : public NiceMock<test::MockAudioDeviceModule> {
 public:
  PlatformAudioProcessingTopology GetPlatformAudioProcessingTopology()
      const override {
    return PlatformAudioProcessingTopology::
        kEchoCancellationAndNoiseSuppressionCoupled;
  }

  PlatformAudioProcessingState GetPlatformAudioProcessingState()
      const override {
    ++readback_count;
    PlatformAudioProcessingState state;
    state.topology = GetPlatformAudioProcessingTopology();
    state.is_echo_cancellation_active = path_active;
    state.is_noise_suppression_active = path_active;
    state.is_voice_processing_enabled_active = path_active;
    return state;
  }

  bool path_active = false;
  mutable int readback_count = 0;
};

// Every SDP exchange re-applies each voice channel's options, and a
// receive-only participant never sets any processing option. That apply must
// not consult the device module, since the readback queries the live engine.
TEST(ApplyAudioProcessingOptionsTest,
     OptionsWithoutProcessingRequestSkipPlatformReadback) {
  auto adm = make_ref_counted<CoupledPlatformAudioDeviceModule>();
  AudioOptions options;
  options.stereo_swapping = true;

  AudioProcessingApplyResult result =
      ApplyAudioProcessingOptions(nullptr, adm.get(), options);

  EXPECT_TRUE(result.result.ok());
  EXPECT_EQ(adm->readback_count, 0);
  EXPECT_EQ(result.resolved_options, options);
}

TEST(ApplyAudioProcessingOptionsTest, EmptyOptionsSkipPlatformReadback) {
  auto adm = make_ref_counted<CoupledPlatformAudioDeviceModule>();

  AudioProcessingApplyResult result =
      ApplyAudioProcessingOptions(nullptr, adm.get(), AudioOptions());

  EXPECT_TRUE(result.result.ok());
  EXPECT_EQ(adm->readback_count, 0);
}

// AGC alone never turns the shared path on, so its software fallback depends
// on the live path state, which is the one case that still needs a readback.
TEST(ApplyAudioProcessingOptionsTest, AgcOnlyOptionsReadPlatformStateOnce) {
  auto adm = make_ref_counted<CoupledPlatformAudioDeviceModule>();
  adm->path_active = true;
  ON_CALL(*adm, BuiltInAGCIsAvailable()).WillByDefault(Return(true));
  ON_CALL(*adm, EnableBuiltInAGC(true)).WillByDefault(Return(0));
  AudioOptions options;
  options.auto_gain_control = true;

  AudioProcessingApplyResult result =
      ApplyAudioProcessingOptions(nullptr, adm.get(), options);

  EXPECT_TRUE(result.result.ok());
  EXPECT_EQ(adm->readback_count, 1);
  // Platform AGC took the request, so software AGC stays off.
  EXPECT_EQ(result.resolved_options.auto_gain_control,
            std::optional<bool>(false));
}

}  // namespace
}  // namespace webrtc
