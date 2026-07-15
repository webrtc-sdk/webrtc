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

#include <string>
#include <utility>

namespace webrtc {
namespace {

bool PlatformPathWouldBeActive(const AudioOptions &options, const AudioProcessingOptionsValidationContext &context) {
  CoupledAudioProcessingPathResolution resolution =
      ResolveCoupledAudioProcessingPath(options, [&context] { return context.is_echo_noise_platform_path_active; });
  return resolution.should_use_echo_noise_platform_path;
}

AudioProcessingOptionsResult RejectInvalidCombination(const char *message) {
  return AudioProcessingOptionsResult::Rejected(AudioProcessingOptionsResultCode::kRejectedInvalidCombination, message);
}

AudioProcessingOptionsResult RejectPlatformUnavailable(const char *message) {
  return AudioProcessingOptionsResult::Rejected(AudioProcessingOptionsResultCode::kRejectedPlatformUnavailable,
                                                message);
}

bool AudioProcessingOptionIsDisabled(std::optional<bool> enabled) { return enabled.has_value() && !*enabled; }

bool CoupledEchoNoisePlatformPathIsActive(const AudioDeviceModule::PlatformAudioProcessingState &state) {
  if (state.is_echo_cancellation_active.has_value() || state.is_noise_suppression_active.has_value()) {
    return state.is_echo_cancellation_active.value_or(false) || state.is_noise_suppression_active.value_or(false);
  }

  return (state.is_echo_cancellation_available || state.is_noise_suppression_available) &&
         (state.is_echo_cancellation_requested.value_or(false) || state.is_noise_suppression_requested.value_or(false));
}

AudioProcessingOptionsResult ValidatePlatformOnlyComponent(std::optional<bool> enabled,
                                                           std::optional<AudioProcessingMode> mode, bool is_available,
                                                           const char *component) {
  if (!AudioProcessingOptionIsPlatformOnly(enabled, mode)) {
    return AudioProcessingOptionsResult::Applied();
  }
  if (!is_available) {
    std::string message = "Platform ";
    message += component;
    message += " processing is not available";
    return AudioProcessingOptionsResult::Rejected(AudioProcessingOptionsResultCode::kRejectedPlatformUnavailable,
                                                  std::move(message));
  }
  return AudioProcessingOptionsResult::Applied();
}

AudioProcessingOptionsResult ValidateCoupledEchoNoiseOptions(const AudioOptions &options,
                                                             const AudioProcessingOptionsValidationContext &context) {
  if (AudioProcessingOptionIsPlatformOnly(options.echo_cancellation, options.echo_cancellation_mode) &&
      AudioProcessingOptionRequestsSoftware(options.noise_suppression, options.noise_suppression_mode)) {
    return RejectInvalidCombination("Platform echo cancellation cannot be combined with software noise suppression");
  }
  if (AudioProcessingOptionIsPlatformOnly(options.echo_cancellation, options.echo_cancellation_mode) &&
      AudioProcessingOptionIsDisabled(options.noise_suppression)) {
    return RejectInvalidCombination("Platform echo cancellation cannot be combined with disabled noise suppression");
  }
  if (AudioProcessingOptionIsPlatformOnly(options.noise_suppression, options.noise_suppression_mode) &&
      AudioProcessingOptionRequestsSoftware(options.echo_cancellation, options.echo_cancellation_mode)) {
    return RejectInvalidCombination("Platform noise suppression cannot be combined with software echo cancellation");
  }
  if (AudioProcessingOptionIsPlatformOnly(options.noise_suppression, options.noise_suppression_mode) &&
      AudioProcessingOptionIsDisabled(options.echo_cancellation)) {
    return RejectInvalidCombination("Platform noise suppression cannot be combined with disabled echo cancellation");
  }

  if ((AudioProcessingOptionIsPlatformOnly(options.echo_cancellation, options.echo_cancellation_mode) ||
       AudioProcessingOptionIsPlatformOnly(options.noise_suppression, options.noise_suppression_mode)) &&
      !context.is_echo_noise_platform_path_available) {
    return RejectPlatformUnavailable("Platform AEC/NS path is not available");
  }

  if (AudioProcessingOptionIsPlatformOnly(options.auto_gain_control, options.auto_gain_control_mode)) {
    if (!PlatformPathWouldBeActive(options, context)) {
      return RejectInvalidCombination("Platform automatic gain control requires the shared AEC/NS platform path");
    }
    if (!context.is_echo_noise_platform_path_available || !context.is_auto_gain_control_platform_available) {
      return RejectPlatformUnavailable("Platform automatic gain control is not available");
    }
  }

  return AudioProcessingOptionsResult::Applied();
}

AudioProcessingOptionsResult ValidateIndependentOptions(const AudioOptions &options,
                                                        const AudioProcessingOptionsValidationContext &context) {
  AudioProcessingOptionsResult echo_result =
      ValidatePlatformOnlyComponent(options.echo_cancellation, options.echo_cancellation_mode,
                                    context.is_echo_cancellation_platform_available, "echo cancellation");
  if (!echo_result.ok()) {
    return echo_result;
  }
  AudioProcessingOptionsResult noise_result =
      ValidatePlatformOnlyComponent(options.noise_suppression, options.noise_suppression_mode,
                                    context.is_noise_suppression_platform_available, "noise suppression");
  if (!noise_result.ok()) {
    return noise_result;
  }
  return ValidatePlatformOnlyComponent(options.auto_gain_control, options.auto_gain_control_mode,
                                       context.is_auto_gain_control_platform_available, "automatic gain control");
}

}  // namespace

AudioProcessingMode AudioProcessingModeOrAutomatic(std::optional<AudioProcessingMode> mode) {
  return mode.value_or(AudioProcessingMode::kAutomatic);
}

bool AudioProcessingOptionWantsPlatform(std::optional<bool> enabled, std::optional<AudioProcessingMode> mode) {
  if (!enabled.value_or(false)) {
    return false;
  }
  AudioProcessingMode processing_mode = AudioProcessingModeOrAutomatic(mode);
  return processing_mode == AudioProcessingMode::kAutomatic || processing_mode == AudioProcessingMode::kPlatform;
}

bool AudioProcessingOptionRequestsSoftware(std::optional<bool> enabled, std::optional<AudioProcessingMode> mode) {
  return enabled.value_or(false) && AudioProcessingModeOrAutomatic(mode) == AudioProcessingMode::kSoftware;
}

bool AudioProcessingOptionIsPlatformOnly(std::optional<bool> enabled, std::optional<AudioProcessingMode> mode) {
  return enabled.value_or(false) && AudioProcessingModeOrAutomatic(mode) == AudioProcessingMode::kPlatform;
}

std::optional<bool> ResolveAudioProcessingSoftwareFromPlatformState(std::optional<bool> enabled,
                                                                    std::optional<AudioProcessingMode> mode,
                                                                    bool platform_enabled) {
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
  resolution.has_echo_or_noise_option = options.echo_cancellation.has_value() || options.noise_suppression.has_value();

  if (resolution.has_echo_or_noise_option) {
    const bool echo_or_noise_requests_software =
        AudioProcessingOptionRequestsSoftware(options.echo_cancellation, options.echo_cancellation_mode) ||
        AudioProcessingOptionRequestsSoftware(options.noise_suppression, options.noise_suppression_mode);
    const bool echo_or_noise_has_disabled_component = AudioProcessingOptionIsDisabled(options.echo_cancellation) ||
                                                      AudioProcessingOptionIsDisabled(options.noise_suppression);
    const bool echo_or_noise_wants_platform =
        AudioProcessingOptionWantsPlatform(options.echo_cancellation, options.echo_cancellation_mode) ||
        AudioProcessingOptionWantsPlatform(options.noise_suppression, options.noise_suppression_mode);
    resolution.should_use_echo_noise_platform_path =
        echo_or_noise_wants_platform && !echo_or_noise_requests_software && !echo_or_noise_has_disabled_component;
  } else {
    resolution.should_use_echo_noise_platform_path = is_echo_noise_platform_path_active();
  }

  resolution.auto_gain_control_wants_platform =
      AudioProcessingOptionWantsPlatform(options.auto_gain_control, options.auto_gain_control_mode);
  return resolution;
}

AudioProcessingOptionsResult ValidateAudioProcessingOptions(const AudioOptions &options,
                                                            const AudioProcessingOptionsValidationContext &context) {
  if (AudioProcessingOptionIsPlatformOnly(options.highpass_filter, options.highpass_filter_mode)) {
    if (!context.is_highpass_filter_platform_available) {
      return RejectInvalidCombination("Platform high-pass filter is not supported");
    }
  }

  if (context.topology ==
      AudioDeviceModule::PlatformAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled) {
    return ValidateCoupledEchoNoiseOptions(options, context);
  }

  return ValidateIndependentOptions(options, context);
}

AudioProcessingOptionsValidationContext AudioProcessingValidationContextForAudioDeviceModule(
    const AudioDeviceModule *adm) {
  AudioProcessingOptionsValidationContext context;
  if (adm == nullptr) {
    return context;
  }

  context.topology = adm->GetPlatformAudioProcessingTopology();
  if (context.topology ==
      AudioDeviceModule::PlatformAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled) {
    const bool path_available = adm->PlatformVoiceProcessingPathIsAvailable();
    context.is_echo_noise_platform_path_available = path_available;
    context.is_echo_noise_platform_path_active =
        CoupledEchoNoisePlatformPathIsActive(adm->GetPlatformAudioProcessingState());
    context.is_echo_cancellation_platform_available = path_available;
    context.is_noise_suppression_platform_available = path_available;
    context.is_auto_gain_control_platform_available = path_available;
    return context;
  }

  context.is_echo_cancellation_platform_available = adm->BuiltInAECIsAvailable();
  context.is_noise_suppression_platform_available = adm->BuiltInNSIsAvailable();
  context.is_auto_gain_control_platform_available = adm->BuiltInAGCIsAvailable();
  return context;
}

}  // namespace webrtc
