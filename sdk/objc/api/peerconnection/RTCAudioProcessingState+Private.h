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

#ifndef SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_PRIVATE_H_
#define SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_PRIVATE_H_

#include <optional>

#include "api/audio/audio_device.h"
#import "sdk/objc/api/peerconnection/RTCAudioDeviceModule.h"

namespace webrtc {
namespace objc {

inline RTC_OBJC_TYPE(RTCOptionalBool) OptionalBoolToObjC(std::optional<bool> value) {
  if (!value.has_value()) {
    return RTC_OBJC_TYPE(RTCOptionalBoolUnknown);
  }
  return value.value() ? RTC_OBJC_TYPE(RTCOptionalBoolYes) : RTC_OBJC_TYPE(RTCOptionalBoolNo);
}

inline RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopology) BuiltInAudioProcessingTopologyToObjC(
    AudioDeviceModule::BuiltInAudioProcessingTopology topology) {
  switch (topology) {
    case AudioDeviceModule::BuiltInAudioProcessingTopology::
        kEchoCancellationAndNoiseSuppressionCoupled:
      return RTC_OBJC_TYPE(
          RTCBuiltInAudioProcessingTopologyEchoCancellationAndNoiseSuppressionCoupled);
    case AudioDeviceModule::BuiltInAudioProcessingTopology::kIndependent:
      return RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopologyIndependent);
  }
}

inline RTC_OBJC_TYPE(RTCBuiltInAudioProcessingComponentState)
    BuiltInAudioProcessingComponentStateToObjC(bool is_available, std::optional<bool> is_requested,
                                               std::optional<bool> is_active) {
  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingComponentState) result;
  result.isAvailable = is_available;
  result.requested = OptionalBoolToObjC(is_requested);
  result.active = OptionalBoolToObjC(is_active);
  return result;
}

inline RTC_OBJC_TYPE(RTCBuiltInAudioProcessingState)
    BuiltInAudioProcessingStateToObjC(const AudioDeviceModule::BuiltInAudioProcessingState &state) {
  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingState) result;
  result.topology = BuiltInAudioProcessingTopologyToObjC(state.topology);
  result.echoCancellation = BuiltInAudioProcessingComponentStateToObjC(
      state.is_echo_cancellation_available, state.is_echo_cancellation_requested, state.is_echo_cancellation_active);
  result.noiseSuppression = BuiltInAudioProcessingComponentStateToObjC(
      state.is_noise_suppression_available, state.is_noise_suppression_requested, state.is_noise_suppression_active);
  result.autoGainControl = BuiltInAudioProcessingComponentStateToObjC(
      state.is_auto_gain_control_available, state.is_auto_gain_control_requested, state.is_auto_gain_control_active);
  result.voiceProcessingEnabledRequested = OptionalBoolToObjC(state.is_voice_processing_enabled_requested);
  result.voiceProcessingBypassedRequested = OptionalBoolToObjC(state.is_voice_processing_bypassed_requested);
  result.voiceProcessingAGCEnabledRequested = OptionalBoolToObjC(state.is_voice_processing_agc_enabled_requested);
  result.voiceProcessingEnabledActive = OptionalBoolToObjC(state.is_voice_processing_enabled_active);
  result.voiceProcessingBypassedActive = OptionalBoolToObjC(state.is_voice_processing_bypassed_active);
  result.voiceProcessingAGCEnabledActive = OptionalBoolToObjC(state.is_voice_processing_agc_enabled_active);
  return result;
}

}  // namespace objc
}  // namespace webrtc

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_PRIVATE_H_
