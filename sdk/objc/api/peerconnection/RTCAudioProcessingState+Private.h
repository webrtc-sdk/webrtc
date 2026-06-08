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
#include "api/audio/audio_processing_runtime_state.h"
#import "sdk/objc/api/peerconnection/RTCAudioDeviceModule.h"
#import "sdk/objc/api/peerconnection/RTCPeerConnection.h"

namespace webrtc {
namespace objc {

inline RTC_OBJC_TYPE(RTCOptionalBool) OptionalBoolToObjC(std::optional<bool> value) {
  if (!value.has_value()) {
    return RTC_OBJC_TYPE(RTCOptionalBoolUnknown);
  }
  return value.value() ? RTC_OBJC_TYPE(RTCOptionalBoolYes) : RTC_OBJC_TYPE(RTCOptionalBoolNo);
}

inline RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopology)
    BuiltInAudioProcessingTopologyToObjC(AudioDeviceModule::BuiltInAudioProcessingTopology topology) {
  switch (topology) {
    case AudioDeviceModule::BuiltInAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled:
      return RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopologyEchoCancellationAndNoiseSuppressionCoupled);
    case AudioDeviceModule::BuiltInAudioProcessingTopology::kIndependent:
      return RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopologyIndependent);
  }
}

inline RTC_OBJC_TYPE(RTCAudioProcessingMode) AudioProcessingModeToObjC(AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kPlatform:
      return RTC_OBJC_TYPE(RTCAudioProcessingModePlatform);
    case AudioProcessingMode::kSoftware:
      return RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware);
    case AudioProcessingMode::kAutomatic:
      return RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic);
  }
}

inline RTC_OBJC_TYPE(RTCAudioProcessingImplementation)
    AudioProcessingImplementationToObjC(AudioProcessingImplementation implementation) {
  switch (implementation) {
    case AudioProcessingImplementation::kDisabled:
      return RTC_OBJC_TYPE(RTCAudioProcessingImplementationDisabled);
    case AudioProcessingImplementation::kSoftware:
      return RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftware);
    case AudioProcessingImplementation::kPlatform:
      return RTC_OBJC_TYPE(RTCAudioProcessingImplementationPlatform);
    case AudioProcessingImplementation::kSoftwareAndPlatform:
      return RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftwareAndPlatform);
    case AudioProcessingImplementation::kUnknown:
      return RTC_OBJC_TYPE(RTCAudioProcessingImplementationUnknown);
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

inline RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState)
    AudioProcessingComponentRuntimeStateToObjC(const AudioProcessingComponentRuntimeState &state) {
  RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState) result;
  result.requestedEnabled = OptionalBoolToObjC(state.is_requested_enabled);
  result.hasRequestedMode = state.requested_mode.has_value();
  result.requestedMode = state.requested_mode.has_value() ? AudioProcessingModeToObjC(*state.requested_mode)
                                                          : RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic);
  result.resolvedSoftwareEnabled = OptionalBoolToObjC(state.is_resolved_software_enabled);
  result.softwareEnabled = OptionalBoolToObjC(state.is_software_enabled);
  result.isPlatformAvailable = state.is_platform_available;
  result.platformRequested = OptionalBoolToObjC(state.is_platform_requested);
  result.platformActive = OptionalBoolToObjC(state.is_platform_active);
  result.effective = AudioProcessingImplementationToObjC(state.effective);
  return result;
}

inline RTC_OBJC_TYPE(RTCAudioProcessingRuntimeState)
    AudioProcessingRuntimeStateToObjC(const AudioProcessingRuntimeState &state) {
  RTC_OBJC_TYPE(RTCAudioProcessingRuntimeState) result;
  result.topology = BuiltInAudioProcessingTopologyToObjC(state.topology);
  result.hasAudioProcessingModule = state.has_audio_processing_module;
  result.hasAudioProcessingConfig = state.has_audio_processing_config;
  result.hasRequestedAudioProcessingOptions = state.has_requested_audio_processing_options;
  result.hasResolvedAudioProcessingOptions = state.has_resolved_audio_processing_options;
  result.echoCancellation = AudioProcessingComponentRuntimeStateToObjC(state.echo_cancellation);
  result.noiseSuppression = AudioProcessingComponentRuntimeStateToObjC(state.noise_suppression);
  result.autoGainControl = AudioProcessingComponentRuntimeStateToObjC(state.auto_gain_control);
  result.highPassFilter = AudioProcessingComponentRuntimeStateToObjC(state.high_pass_filter);
  result.builtIn = BuiltInAudioProcessingStateToObjC(state.built_in);
  return result;
}

}  // namespace objc
}  // namespace webrtc

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_PRIVATE_H_
