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

#include "api/audio/audio_processing_options_resolver.h"

namespace webrtc {

AudioProcessingMode AudioProcessingModeOrAutomatic(std::optional<AudioProcessingMode> mode) {
  return mode.value_or(AudioProcessingMode::kAutomatic);
}

bool AudioProcessingOptionWantsPlatform(std::optional<bool> enabled,
                                        std::optional<AudioProcessingMode> mode) {
  if (!enabled.value_or(false)) {
    return false;
  }
  AudioProcessingMode processing_mode = AudioProcessingModeOrAutomatic(mode);
  return processing_mode == AudioProcessingMode::kAutomatic ||
         processing_mode == AudioProcessingMode::kPlatform;
}

bool AudioProcessingOptionRequestsSoftware(std::optional<bool> enabled,
                                           std::optional<AudioProcessingMode> mode) {
  return enabled.value_or(false) &&
         AudioProcessingModeOrAutomatic(mode) == AudioProcessingMode::kSoftware;
}

bool AudioProcessingOptionIsPlatformOnly(std::optional<bool> enabled,
                                         std::optional<AudioProcessingMode> mode) {
  return enabled.value_or(false) &&
         AudioProcessingModeOrAutomatic(mode) == AudioProcessingMode::kPlatform;
}

std::optional<bool> ResolveAudioProcessingSoftwareFromPlatformState(
    std::optional<bool> enabled, std::optional<AudioProcessingMode> mode, bool platform_enabled) {
  if (!enabled.has_value()) {
    return std::nullopt;
  }
  if (!*enabled) {
    return false;
  }

  switch (AudioProcessingModeOrAutomatic(mode)) {
    case AudioProcessingMode::kAutomatic:
      return !platform_enabled;
    case AudioProcessingMode::kPlatform:
      return false;
    case AudioProcessingMode::kSoftware:
      return true;
  }
  return false;
}

CoupledAudioProcessingPathResolution ResolveCoupledAudioProcessingPath(
    const AudioOptions &options, FunctionView<bool()> is_echo_noise_platform_path_active) {
  CoupledAudioProcessingPathResolution resolution;
  resolution.has_echo_or_noise_option =
      options.echo_cancellation.has_value() || options.noise_suppression.has_value();

  if (resolution.has_echo_or_noise_option) {
    const bool echo_or_noise_requests_software =
        AudioProcessingOptionRequestsSoftware(options.echo_cancellation,
                                              options.echo_cancellation_mode) ||
        AudioProcessingOptionRequestsSoftware(options.noise_suppression,
                                              options.noise_suppression_mode);
    const bool echo_or_noise_wants_platform =
        AudioProcessingOptionWantsPlatform(options.echo_cancellation,
                                           options.echo_cancellation_mode) ||
        AudioProcessingOptionWantsPlatform(options.noise_suppression,
                                           options.noise_suppression_mode);
    resolution.should_use_echo_noise_platform_path =
        echo_or_noise_wants_platform && !echo_or_noise_requests_software;
  } else {
    resolution.should_use_echo_noise_platform_path = is_echo_noise_platform_path_active();
  }

  resolution.auto_gain_control_wants_platform =
      AudioProcessingOptionWantsPlatform(options.auto_gain_control, options.auto_gain_control_mode);
  return resolution;
}

}  // namespace webrtc
