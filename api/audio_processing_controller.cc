/*
 *  Copyright 2026 LiveKit
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "api/audio_processing_controller.h"

#include <optional>

namespace webrtc {
namespace {

void ForceSoftwareProcessing(AudioOptions* options, bool enabled) {
  options->echo_cancellation = enabled;
  options->auto_gain_control = enabled;
  options->noise_suppression = enabled;
  options->highpass_filter = enabled;
}

using AvailabilityFn = bool (AudioDeviceModule::*)() const;
using EnableFn = int32_t (AudioDeviceModule::*)(bool);

bool DisablePlatformEffect(AudioDeviceModule* adm,
                           AvailabilityFn is_available,
                           EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(false) == 0;
}

bool ApplyPlatformEffect(AudioDeviceModule* adm,
                         std::optional<bool>* software_option,
                         AvailabilityFn is_available,
                         EnableFn enable,
                         bool allow_platform,
                         bool require_platform) {
  if (!software_option->has_value()) {
    return false;
  }

  const bool requested = software_option->value();
  if (!requested || !allow_platform) {
    DisablePlatformEffect(adm, is_available, enable);
    return false;
  }

  if (adm != nullptr && (adm->*is_available)() && (adm->*enable)(true) == 0) {
    // ADM-provided processing replaces the corresponding WebRTC APM module.
    *software_option = false;
    return true;
  }

  if (require_platform) {
    // Platform-only mode does not fall back to WebRTC software processing.
    *software_option = false;
  }
  return false;
}

void DisablePlatformAudioProcessing(AudioDeviceModule* adm) {
  DisablePlatformEffect(adm, &AudioDeviceModule::BuiltInAECIsAvailable,
                        &AudioDeviceModule::EnableBuiltInAEC);
  DisablePlatformEffect(adm, &AudioDeviceModule::BuiltInAGCIsAvailable,
                        &AudioDeviceModule::EnableBuiltInAGC);
  DisablePlatformEffect(adm, &AudioDeviceModule::BuiltInNSIsAvailable,
                        &AudioDeviceModule::EnableBuiltInNS);
}

}  // namespace

bool IsAudioProcessingModeValid(AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
    case AudioProcessingMode::kPlatform:
    case AudioProcessingMode::kSoftware:
    case AudioProcessingMode::kDisabled:
      return true;
  }
  return false;
}

bool IsPlatformAudioProcessingAvailable(AudioDeviceModule* adm) {
  return adm != nullptr &&
         (adm->BuiltInAECIsAvailable() || adm->BuiltInAGCIsAvailable() ||
          adm->BuiltInNSIsAvailable());
}

bool ShouldUsePlatformAudioProcessing(AudioDeviceModule* adm,
                                      AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
      return IsPlatformAudioProcessingAvailable(adm);
    case AudioProcessingMode::kPlatform:
      return IsPlatformAudioProcessingAvailable(adm);
    case AudioProcessingMode::kSoftware:
    case AudioProcessingMode::kDisabled:
      return false;
  }
  return false;
}

void ApplyAudioProcessingConfig(AudioProcessing* apm,
                                const AudioOptions& options) {
  if (apm == nullptr) {
    return;
  }

  AudioProcessing::Config apm_config = apm->GetConfig();

  if (options.echo_cancellation) {
    apm_config.echo_canceller.enabled = *options.echo_cancellation;
  }

  if (options.auto_gain_control) {
    const bool enabled = *options.auto_gain_control;
    apm_config.gain_controller1.enabled = enabled;
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC) || defined(WEBRTC_ANDROID)
    apm_config.gain_controller1.mode =
        AudioProcessing::Config::GainController1::kFixedDigital;
#else
    apm_config.gain_controller1.mode =
        AudioProcessing::Config::GainController1::kAdaptiveAnalog;
#endif
  }

  if (options.highpass_filter) {
    apm_config.high_pass_filter.enabled = *options.highpass_filter;
  }

  if (options.noise_suppression) {
    const bool enabled = *options.noise_suppression;
    apm_config.noise_suppression.enabled = enabled;
    apm_config.noise_suppression.level =
        AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  }

  apm->ApplyConfig(apm_config);
}

AudioOptions ApplyAudioProcessingOptions(AudioProcessing* apm,
                                         AudioDeviceModule* adm,
                                         AudioProcessingMode mode,
                                         const AudioOptions& options_in) {
  AudioOptions options = options_in;

  switch (mode) {
    case AudioProcessingMode::kDisabled:
      DisablePlatformAudioProcessing(adm);
      ForceSoftwareProcessing(&options, false);
      break;
    case AudioProcessingMode::kPlatform: {
      const bool platform_aec = ApplyPlatformEffect(
          adm, &options.echo_cancellation,
          &AudioDeviceModule::BuiltInAECIsAvailable,
          &AudioDeviceModule::EnableBuiltInAEC, true, true);
      const bool platform_agc = ApplyPlatformEffect(
          adm, &options.auto_gain_control,
          &AudioDeviceModule::BuiltInAGCIsAvailable,
          &AudioDeviceModule::EnableBuiltInAGC, true, true);
      const bool platform_ns = ApplyPlatformEffect(
          adm, &options.noise_suppression,
          &AudioDeviceModule::BuiltInNSIsAvailable,
          &AudioDeviceModule::EnableBuiltInNS, true, true);
      if (!platform_aec) {
        options.echo_cancellation = false;
      }
      if (!platform_agc) {
        options.auto_gain_control = false;
      }
      if (!platform_ns) {
        options.noise_suppression = false;
      }
      options.highpass_filter = false;
      break;
    }
    case AudioProcessingMode::kAutomatic: {
      ApplyPlatformEffect(
          adm, &options.echo_cancellation,
          &AudioDeviceModule::BuiltInAECIsAvailable,
          &AudioDeviceModule::EnableBuiltInAEC, true, false);
      ApplyPlatformEffect(
          adm, &options.auto_gain_control,
          &AudioDeviceModule::BuiltInAGCIsAvailable,
          &AudioDeviceModule::EnableBuiltInAGC, true, false);
      ApplyPlatformEffect(
          adm, &options.noise_suppression,
          &AudioDeviceModule::BuiltInNSIsAvailable,
          &AudioDeviceModule::EnableBuiltInNS, true, false);
      break;
    }
    case AudioProcessingMode::kSoftware:
      DisablePlatformAudioProcessing(adm);
      break;
  }

  ApplyAudioProcessingConfig(apm, options);
  return options;
}

}  // namespace webrtc
