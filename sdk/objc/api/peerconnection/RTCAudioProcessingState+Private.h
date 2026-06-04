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

inline void SetOptionalBool(std::optional<bool> value, BOOL *has_value, BOOL *output_value) {
  *has_value = value.has_value();
  *output_value = value.value_or(false);
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
                                               std::optional<bool> is_observed) {
  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingComponentState) result;
  result.isAvailable = is_available;
  SetOptionalBool(is_requested, &result.hasRequested, &result.isRequested);
  SetOptionalBool(is_observed, &result.hasObserved, &result.isObserved);
  return result;
}

inline RTC_OBJC_TYPE(RTCBuiltInAudioProcessingState)
    BuiltInAudioProcessingStateToObjC(const AudioDeviceModule::BuiltInAudioProcessingState &state) {
  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingState) result;
  result.topology = BuiltInAudioProcessingTopologyToObjC(state.topology);
  result.echoCancellation = BuiltInAudioProcessingComponentStateToObjC(
      state.is_echo_cancellation_available, state.is_echo_cancellation_requested,
      state.is_echo_cancellation_observed);
  result.noiseSuppression = BuiltInAudioProcessingComponentStateToObjC(
      state.is_noise_suppression_available, state.is_noise_suppression_requested,
      state.is_noise_suppression_observed);
  result.autoGainControl = BuiltInAudioProcessingComponentStateToObjC(
      state.is_auto_gain_control_available, state.is_auto_gain_control_requested,
      state.is_auto_gain_control_observed);
  SetOptionalBool(state.is_voice_processing_enabled_requested,
                  &result.hasVoiceProcessingEnabledRequested,
                  &result.isVoiceProcessingEnabledRequested);
  SetOptionalBool(state.is_voice_processing_bypassed_requested,
                  &result.hasVoiceProcessingBypassedRequested,
                  &result.isVoiceProcessingBypassedRequested);
  SetOptionalBool(state.is_voice_processing_agc_enabled_requested,
                  &result.hasVoiceProcessingAGCEnabledRequested,
                  &result.isVoiceProcessingAGCEnabledRequested);
  SetOptionalBool(state.is_voice_processing_enabled_observed,
                  &result.hasVoiceProcessingEnabledObserved,
                  &result.isVoiceProcessingEnabledObserved);
  SetOptionalBool(state.is_voice_processing_bypassed_observed,
                  &result.hasVoiceProcessingBypassedObserved,
                  &result.isVoiceProcessingBypassedObserved);
  SetOptionalBool(state.is_voice_processing_agc_enabled_observed,
                  &result.hasVoiceProcessingAGCEnabledObserved,
                  &result.isVoiceProcessingAGCEnabledObserved);
  return result;
}

}  // namespace objc
}  // namespace webrtc

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_PRIVATE_H_
