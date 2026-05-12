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

bool AnySoftwareProcessingEnabled(const AudioOptions& options) {
  return options.echo_cancellation.value_or(false) ||
         options.auto_gain_control.value_or(false) ||
         options.noise_suppression.value_or(false) ||
         options.highpass_filter.value_or(false);
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

AudioProcessingBackend ResolveAudioProcessingBackend(
    AudioDeviceModule* adm,
    AudioProcessingMode mode,
    const AudioOptions& resolved_options,
    int32_t* error) {
  if (error != nullptr) {
    *error = 0;
  }
  switch (mode) {
    case AudioProcessingMode::kDisabled:
      return AudioProcessingBackend::kDisabled;

    case AudioProcessingMode::kSoftware:
      return AnySoftwareProcessingEnabled(resolved_options)
                 ? AudioProcessingBackend::kSoftware
                 : AudioProcessingBackend::kDisabled;

    case AudioProcessingMode::kSystem:
      if (!IsSystemAudioProcessingAvailable(adm)) {
        if (error != nullptr) {
          *error = -1;
        }
        return AudioProcessingBackend::kUnavailable;
      }
      return AudioProcessingBackend::kSystem;

    case AudioProcessingMode::kAutomatic:
      if (IsSystemAudioProcessingAvailable(adm)) {
        return AudioProcessingBackend::kSystem;
      }
      return AnySoftwareProcessingEnabled(resolved_options)
                 ? AudioProcessingBackend::kSoftware
                 : AudioProcessingBackend::kDisabled;
  }
  return AudioProcessingBackend::kUnavailable;
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
                                         const AudioOptions& options_in,
                                         AudioProcessingState* state) {
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

  int32_t error = 0;
  const AudioProcessingBackend backend =
      ResolveAudioProcessingBackend(adm, mode, options, &error);

  ApplyAudioProcessingConfig(apm, options);

  if (state != nullptr) {
    state->requested_mode = mode;
    state->backend = backend;
    state->transition_from = mode;
    state->transition_to = mode;
    state->last_error = error;
    state->software_echo_cancellation =
        options.echo_cancellation.value_or(false);
    state->software_auto_gain_control =
        options.auto_gain_control.value_or(false);
    state->software_noise_suppression =
        options.noise_suppression.value_or(false);
    state->software_highpass_filter = options.highpass_filter.value_or(false);
    state->lifecycle =
        error == 0 ? ((adm != nullptr && (adm->Playing() || adm->Recording()))
                          ? AudioProcessingLifecycle::kRunning
                          : AudioProcessingLifecycle::kIdle)
                   : AudioProcessingLifecycle::kFailed;
  }

  return options;
}

}  // namespace webrtc
