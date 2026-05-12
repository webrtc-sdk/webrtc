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

namespace webrtc {
namespace {

void ForceSoftwareProcessing(AudioOptions* options, bool enabled) {
  options->echo_cancellation = enabled;
  options->auto_gain_control = enabled;
  options->noise_suppression = enabled;
  options->highpass_filter = enabled;
}

}  // namespace

bool IsAudioProcessingModeValid(AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
    case AudioProcessingMode::kSystem:
    case AudioProcessingMode::kSoftware:
    case AudioProcessingMode::kDisabled:
      return true;
  }
  return false;
}

bool IsSystemAudioProcessingAvailable(AudioDeviceModule* adm) {
  return adm != nullptr && adm->BuiltInAECIsAvailable();
}

bool ShouldUseSystemAudioProcessing(AudioDeviceModule* adm,
                                    AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
      return IsSystemAudioProcessingAvailable(adm);
    case AudioProcessingMode::kSystem:
      return IsSystemAudioProcessingAvailable(adm);
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
      ForceSoftwareProcessing(&options, false);
      break;
    case AudioProcessingMode::kSystem:
      ForceSoftwareProcessing(&options, false);
      break;
    case AudioProcessingMode::kAutomatic:
      if (IsSystemAudioProcessingAvailable(adm)) {
        ForceSoftwareProcessing(&options, false);
      }
      break;
    case AudioProcessingMode::kSoftware:
      break;
  }

  ApplyAudioProcessingConfig(apm, options);
  return options;
}

}  // namespace webrtc
