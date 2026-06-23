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

#include "media/engine/audio_processing_controller.h"

#include <optional>
#include <string>

#include "api/audio/audio_processing_options_resolver.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

using AvailabilityFn = bool (AudioDeviceModule::*)() const;
using EnableFn = int32_t (AudioDeviceModule::*)(bool);

enum class PlatformEffectEnableResult {
  kEnabled,
  kNoAudioDeviceModule,
  kUnavailable,
  kEnableFailed,
};

std::string PlatformOnlyRequestDisabledMessage(const char *component, const char *reason) {
  std::string message = "Requested platform ";
  message += component;
  message += " processing, but ";
  message += reason;
  message += ". Software fallback is disabled by platform mode.";
  return message;
}

void LogPlatformOnlyRequestDisabled(const char *component, const char *reason) {
  RTC_LOG(LS_WARNING) << PlatformOnlyRequestDisabledMessage(component, reason);
}

const char *PlatformEffectEnableFailureReason(PlatformEffectEnableResult result) {
  switch (result) {
    case PlatformEffectEnableResult::kNoAudioDeviceModule:
      return "there is no audio device module";
    case PlatformEffectEnableResult::kUnavailable:
      return "platform processing is not available";
    case PlatformEffectEnableResult::kEnableFailed:
      return "the audio device module rejected the enable request";
    case PlatformEffectEnableResult::kEnabled:
      return "platform processing is enabled";
  }
  return "platform processing could not be enabled";
}

bool DisablePlatformEffect(AudioDeviceModule *adm, AvailabilityFn is_available, EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(false) == 0;
}

bool EnablePlatformEffect(AudioDeviceModule *adm, AvailabilityFn is_available, EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(true) == 0;
}

PlatformEffectEnableResult TryEnablePlatformEffect(AudioDeviceModule *adm, AvailabilityFn is_available,
                                                   EnableFn enable) {
  if (adm == nullptr) {
    return PlatformEffectEnableResult::kNoAudioDeviceModule;
  }
  if (!(adm->*is_available)()) {
    return PlatformEffectEnableResult::kUnavailable;
  }
  if ((adm->*enable)(true) != 0) {
    return PlatformEffectEnableResult::kEnableFailed;
  }
  return PlatformEffectEnableResult::kEnabled;
}

std::optional<bool> ApplyIndependentPlatformEffectAndResolveSoftware(std::optional<bool> enabled,
                                                                     std::optional<AudioProcessingMode> mode,
                                                                     const char *component, AudioDeviceModule *adm,
                                                                     AvailabilityFn is_available, EnableFn enable) {
  if (!enabled.has_value()) {
    return std::nullopt;
  }
  if (!*enabled) {
    DisablePlatformEffect(adm, is_available, enable);
    return false;
  }

  switch (AudioProcessingModeOrAutomatic(mode)) {
    case AudioProcessingMode::kAutomatic:
      return !EnablePlatformEffect(adm, is_available, enable);
    case AudioProcessingMode::kPlatform: {
      const PlatformEffectEnableResult platform_enable_result = TryEnablePlatformEffect(adm, is_available, enable);
      if (platform_enable_result != PlatformEffectEnableResult::kEnabled) {
        LogPlatformOnlyRequestDisabled(component, PlatformEffectEnableFailureReason(platform_enable_result));
        return false;
      }
      return false;
    }
    case AudioProcessingMode::kSoftware:
      DisablePlatformEffect(adm, is_available, enable);
      return true;
  }
  return false;
}

bool PlatformEffectIsAvailable(AudioDeviceModule *adm, AvailabilityFn is_available) {
  return adm != nullptr && (adm->*is_available)();
}

bool SetPlatformEffect(AudioDeviceModule *adm, EnableFn enable, bool enabled) {
  return adm != nullptr && (adm->*enable)(enabled) == 0;
}

bool CoupledEchoNoisePlatformPathIsActive(AudioDeviceModule *adm) {
  return AudioProcessingValidationContextForAudioDeviceModule(adm).is_echo_noise_platform_path_active;
}

bool PlatformVoiceProcessingPathIsAvailable(AudioDeviceModule *adm) {
  return adm != nullptr && adm->PlatformVoiceProcessingPathIsAvailable();
}

bool SetPlatformVoiceProcessingPath(AudioDeviceModule *adm, bool enabled) {
  return adm != nullptr && adm->EnablePlatformVoiceProcessingPath(enabled) == 0;
}

bool ResolveHighPassFilter(std::optional<bool> enabled, std::optional<AudioProcessingMode> mode) {
  // No supported platform HPF exists today. Validation rejects platform HPF
  // before normal engine application. Keep this strict as a fallback for direct
  // test calls that bypass validation.
  if (!enabled.has_value() || !*enabled) {
    return false;
  }
  if (AudioProcessingOptionIsPlatformOnly(enabled, mode)) {
    LogPlatformOnlyRequestDisabled("high-pass filter", "no platform high-pass filter is available");
  }
  return AudioProcessingModeOrAutomatic(mode) != AudioProcessingMode::kPlatform;
}

AudioProcessingImplementation ResolveEffectiveImplementation(std::optional<bool> software_active,
                                                             bool platform_available,
                                                             std::optional<bool> platform_resolved,
                                                             std::optional<bool> platform_active) {
  const bool software_on = software_active.value_or(false);
  const bool platform_on =
      platform_active.value_or(platform_available && platform_resolved.value_or(false));

  if (software_on && platform_on) {
    return AudioProcessingImplementation::kSoftwareAndPlatform;
  }
  if (software_on) {
    return AudioProcessingImplementation::kSoftware;
  }
  if (platform_on) {
    return AudioProcessingImplementation::kPlatform;
  }
  if (software_active.has_value() || platform_resolved.has_value() || platform_active.has_value() ||
      platform_available) {
    return AudioProcessingImplementation::kDisabled;
  }
  return AudioProcessingImplementation::kUnknown;
}

AudioProcessingComponentState BuildComponentState(
    std::optional<bool> requested_enabled, std::optional<AudioProcessingMode> requested_mode,
    std::optional<bool> software_resolved, std::optional<bool> software_active,
    bool platform_available, std::optional<bool> platform_resolved, std::optional<bool> platform_active) {
  AudioProcessingComponentState state;
  state.requested_enabled = requested_enabled;
  state.requested_mode = requested_mode;
  state.software_resolved = software_resolved;
  state.software_active = software_active;
  state.platform_available = platform_available;
  state.platform_resolved = platform_resolved;
  state.platform_active = platform_active;
  state.effective = ResolveEffectiveImplementation(state.software_active, state.platform_available,
                                                   state.platform_resolved, state.platform_active);
  return state;
}

AudioProcessingApplyResult ApplyCoupledEchoNoiseProcessingOptions(AudioDeviceModule *adm,
                                                                  const AudioOptions &options_in) {
  // Apple Voice Processing I/O exposes AEC and NS through one shared path.
  // Bypassing that path at runtime can leave CoreAudio processing through a
  // VPIO-shaped route while WebRTC APM is also active. For coupled topologies,
  // software or disabled AEC/NS removes the built-in path entirely. Platform
  // AEC/NS recreates the path and then enables both shared effects.
  //
  // A single AEC or NS automatic/platform request can turn the shared path on
  // for both effects. A disabled or software sibling keeps the shared path off
  // so the effective state does not enable processing that the caller disabled
  // or mix Apple's coupled processing with WebRTC APM. Strict platform requests
  // with a disabled or software sibling are rejected before this function is
  // called. AGC is resolved after that decision and never turns VPIO on by
  // itself. This keeps partial AGC-only updates from changing Apple audio
  // routing.
  AudioProcessingApplyResult apply_result;
  apply_result.resolved_options = options_in;
  AudioOptions &software_options = apply_result.resolved_options;

  CoupledAudioProcessingPathResolution path_resolution =
      ResolveCoupledAudioProcessingPath(options_in, [adm] { return CoupledEchoNoisePlatformPathIsActive(adm); });
  bool vpio_enabled = false;

  if (path_resolution.has_echo_or_noise_option) {
    const bool path_available = PlatformVoiceProcessingPathIsAvailable(adm);
    const bool should_enable_vpio = path_available && path_resolution.should_use_echo_noise_platform_path;

    if (should_enable_vpio) {
      const bool path_enabled = SetPlatformVoiceProcessingPath(adm, true);
      const bool effects_available = path_enabled &&
                                     PlatformEffectIsAvailable(adm, &AudioDeviceModule::BuiltInAECIsAvailable) &&
                                     PlatformEffectIsAvailable(adm, &AudioDeviceModule::BuiltInNSIsAvailable);
      if (effects_available) {
        const bool aec_updated = SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInAEC, true);
        const bool ns_updated = SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInNS, true);
        vpio_enabled = aec_updated && ns_updated;
      }
      if (!vpio_enabled) {
        // Treat coupled AEC and NS as an all-or-nothing platform path. If the
        // path or either effect cannot be enabled, remove the path before
        // falling back to software to avoid mixing platform and WebRTC APM.
        if (path_enabled) {
          SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInAEC, false);
          SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInNS, false);
          SetPlatformVoiceProcessingPath(adm, false);
        }
      }
    } else if (path_available) {
      SetPlatformVoiceProcessingPath(adm, false);
    }
  } else {
    vpio_enabled = path_resolution.should_use_echo_noise_platform_path;
  }

  if (!vpio_enabled) {
    if (AudioProcessingOptionIsPlatformOnly(options_in.echo_cancellation, options_in.echo_cancellation_mode)) {
      LogPlatformOnlyRequestDisabled("echo cancellation", "the coupled platform processing path could not be enabled");
    }
    if (AudioProcessingOptionIsPlatformOnly(options_in.noise_suppression, options_in.noise_suppression_mode)) {
      LogPlatformOnlyRequestDisabled("noise suppression", "the coupled platform processing path could not be enabled");
    }
  }

  if (options_in.echo_cancellation.has_value()) {
    software_options.echo_cancellation = ResolveAudioProcessingSoftwareFromPlatformState(
        options_in.echo_cancellation, options_in.echo_cancellation_mode, vpio_enabled);
  }

  if (options_in.noise_suppression.has_value()) {
    software_options.noise_suppression = ResolveAudioProcessingSoftwareFromPlatformState(
        options_in.noise_suppression, options_in.noise_suppression_mode, vpio_enabled);
  }

  if (options_in.auto_gain_control.has_value()) {
    // Apple AGC has its own switch, but it only has effect while the shared
    // AEC/NS VPIO path is active. AGC alone never enables VPIO, so `auto`
    // falls back to software when AEC/NS did not enable the shared path.
    const bool agc_switch_available = PlatformEffectIsAvailable(adm, &AudioDeviceModule::BuiltInAGCIsAvailable);
    const bool should_enable_agc_platform =
        vpio_enabled && agc_switch_available && path_resolution.auto_gain_control_wants_platform;
    // Track the effective platform state, not just the requested state. Apple
    // may reject AGC enable even while VPIO is active, and `auto` must fall
    // back to software in that case.
    bool agc_platform_enabled = false;
    if (agc_switch_available) {
      if (should_enable_agc_platform) {
        agc_platform_enabled = SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInAGC, true);
        if (!agc_platform_enabled) {
          // Keep AGC off when enabling it fails. This keeps `auto` honest about
          // the effective platform state and lets software AGC take over.
          SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInAGC, false);
        }
      } else {
        SetPlatformEffect(adm, &AudioDeviceModule::EnableBuiltInAGC, false);
      }
    }
    if (AudioProcessingOptionIsPlatformOnly(options_in.auto_gain_control, options_in.auto_gain_control_mode) &&
        !agc_platform_enabled) {
      LogPlatformOnlyRequestDisabled("auto gain control", "Apple AGC could not be enabled");
    }
    software_options.auto_gain_control = ResolveAudioProcessingSoftwareFromPlatformState(
        options_in.auto_gain_control, options_in.auto_gain_control_mode, agc_platform_enabled);
  }

  return apply_result;
}

std::optional<AudioProcessing::Config> GetApmConfig(AudioProcessing *apm) {
  if (apm == nullptr) {
    return std::nullopt;
  }
  return apm->GetConfig();
}

}  // namespace

AudioProcessingOptionsResult ValidateAudioProcessingOptionsForApply(AudioDeviceModule *adm,
                                                                    const AudioOptions &options) {
  return ValidateAudioProcessingOptions(options, AudioProcessingValidationContextForAudioDeviceModule(adm));
}

AudioProcessingApplyResult ApplyAudioProcessingOptions(AudioProcessing *apm, AudioDeviceModule *adm,
                                                       const AudioOptions &options_in) {
  AudioProcessingApplyResult apply_result;
  apply_result.resolved_options = options_in;

  if (adm != nullptr &&
      adm->GetPlatformAudioProcessingTopology() ==
          AudioDeviceModule::PlatformAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled) {
    apply_result = ApplyCoupledEchoNoiseProcessingOptions(adm, options_in);
    if (!apply_result.result.ok()) {
      return apply_result;
    }
  } else {
    AudioOptions &software_options = apply_result.resolved_options;
    auto apply_independent_component = [&](std::optional<bool> &software_enabled, std::optional<bool> enabled,
                                           std::optional<AudioProcessingMode> mode, const char *component,
                                           AvailabilityFn is_available, EnableFn enable) {
      if (!enabled.has_value()) {
        return;
      }

      software_enabled =
          ApplyIndependentPlatformEffectAndResolveSoftware(enabled, mode, component, adm, is_available, enable);
    };

    apply_independent_component(software_options.echo_cancellation, options_in.echo_cancellation,
                                options_in.echo_cancellation_mode, "echo cancellation",
                                &AudioDeviceModule::BuiltInAECIsAvailable, &AudioDeviceModule::EnableBuiltInAEC);
    apply_independent_component(software_options.auto_gain_control, options_in.auto_gain_control,
                                options_in.auto_gain_control_mode, "auto gain control",
                                &AudioDeviceModule::BuiltInAGCIsAvailable, &AudioDeviceModule::EnableBuiltInAGC);
    apply_independent_component(software_options.noise_suppression, options_in.noise_suppression,
                                options_in.noise_suppression_mode, "noise suppression",
                                &AudioDeviceModule::BuiltInNSIsAvailable, &AudioDeviceModule::EnableBuiltInNS);
  }

  AudioOptions &software_options = apply_result.resolved_options;
  if (options_in.highpass_filter.has_value()) {
    software_options.highpass_filter =
        ResolveHighPassFilter(options_in.highpass_filter, options_in.highpass_filter_mode);
  }

  if (apm == nullptr) {
    return apply_result;
  }

  AudioProcessing::Config apm_config = apm->GetConfig();

  if (software_options.echo_cancellation.has_value()) {
    apm_config.echo_canceller.enabled = *software_options.echo_cancellation;
  }

  if (software_options.auto_gain_control.has_value()) {
    const bool enabled = *software_options.auto_gain_control;
    apm_config.gain_controller1.enabled = enabled;
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC) || defined(WEBRTC_ANDROID)
    apm_config.gain_controller1.mode = AudioProcessing::Config::GainController1::kFixedDigital;
#else
    apm_config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveAnalog;
#endif
  }

  if (software_options.highpass_filter.has_value()) {
    apm_config.high_pass_filter.enabled = *software_options.highpass_filter;
  }

  if (software_options.noise_suppression.has_value()) {
    const bool enabled = *software_options.noise_suppression;
    apm_config.noise_suppression.enabled = enabled;
    apm_config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  }

  apm->ApplyConfig(apm_config);
  return apply_result;
}

AudioProcessingState GetAudioProcessingState(AudioProcessing *apm, AudioDeviceModule *adm,
                                             const std::optional<AudioOptions> &requested_options,
                                             const std::optional<AudioOptions> &resolved_options) {
  AudioDeviceModule::PlatformAudioProcessingState platform_state;
  if (adm != nullptr) {
    platform_state = adm->GetPlatformAudioProcessingState();
  }
  std::optional<AudioProcessing::Config> apm_config = GetApmConfig(apm);
  std::optional<bool> software_echo_cancellation;
  std::optional<bool> software_noise_suppression;
  std::optional<bool> software_auto_gain_control;
  std::optional<bool> software_high_pass_filter;
  if (apm_config.has_value()) {
    software_echo_cancellation = apm_config->echo_canceller.enabled;
    software_noise_suppression = apm_config->noise_suppression.enabled;
    // Diagnostics report the effective APM state, not only values written by
    // this controller. GC2 may be enabled by other WebRTC configuration paths.
    software_auto_gain_control = apm_config->gain_controller1.enabled || apm_config->gain_controller2.enabled;
    software_high_pass_filter = apm_config->high_pass_filter.enabled;
  }

  AudioProcessingState state;
  state.has_audio_processing_module = apm != nullptr;
  const AudioOptions requested = requested_options.value_or(AudioOptions());
  const AudioOptions resolved = resolved_options.value_or(AudioOptions());
  state.echo_cancellation =
      BuildComponentState(requested.echo_cancellation, requested.echo_cancellation_mode, resolved.echo_cancellation,
                          software_echo_cancellation, platform_state.is_echo_cancellation_available,
                          platform_state.is_echo_cancellation_requested, platform_state.is_echo_cancellation_active);
  state.noise_suppression =
      BuildComponentState(requested.noise_suppression, requested.noise_suppression_mode, resolved.noise_suppression,
                          software_noise_suppression, platform_state.is_noise_suppression_available,
                          platform_state.is_noise_suppression_requested, platform_state.is_noise_suppression_active);
  state.auto_gain_control =
      BuildComponentState(requested.auto_gain_control, requested.auto_gain_control_mode, resolved.auto_gain_control,
                          software_auto_gain_control, platform_state.is_auto_gain_control_available,
                          platform_state.is_auto_gain_control_requested, platform_state.is_auto_gain_control_active);
  state.high_pass_filter =
      BuildComponentState(requested.highpass_filter, requested.highpass_filter_mode, resolved.highpass_filter,
                          software_high_pass_filter, false, std::nullopt, std::nullopt);
  return state;
}

}  // namespace webrtc
