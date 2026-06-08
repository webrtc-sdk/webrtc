/*
 * Copyright 2026 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGOPTIONS_PRIVATE_H_
#define SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGOPTIONS_PRIVATE_H_

#include "api/audio_options.h"
#import "sdk/objc/api/peerconnection/RTCAudioTrack.h"

namespace webrtc {
namespace objc {

inline AudioProcessingMode NativeAudioProcessingMode(RTC_OBJC_TYPE(RTCAudioProcessingMode) mode) {
  switch (mode) {
    case RTC_OBJC_TYPE(RTCAudioProcessingModePlatform):
      return AudioProcessingMode::kPlatform;
    case RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware):
      return AudioProcessingMode::kSoftware;
    case RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic):
    default:
      return AudioProcessingMode::kAutomatic;
  }
}

inline AudioOptions NativeAudioProcessingOptions(RTC_OBJC_TYPE(RTCAudioProcessingOptions) * options) {
  AudioOptions native_options;
  native_options.echo_cancellation = options.echoCancellation;
  native_options.noise_suppression = options.noiseSuppression;
  native_options.auto_gain_control = options.autoGainControl;
  native_options.highpass_filter = options.highPassFilter;
  native_options.echo_cancellation_mode = NativeAudioProcessingMode(options.echoCancellationMode);
  native_options.noise_suppression_mode = NativeAudioProcessingMode(options.noiseSuppressionMode);
  native_options.auto_gain_control_mode = NativeAudioProcessingMode(options.autoGainControlMode);
  native_options.highpass_filter_mode = NativeAudioProcessingMode(options.highPassFilterMode);
  return native_options;
}

}  // namespace objc
}  // namespace webrtc

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGOPTIONS_PRIVATE_H_
