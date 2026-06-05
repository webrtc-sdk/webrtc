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

#include "api/audio/audio_processing_options_resolver.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

using AvailabilityFn = bool (AudioDeviceModule::*)() const;
using EnableFn = int32_t (AudioDeviceModule::*)(bool);

void LogPlatformOnlyRequestDisabled(const char* component, const char* reason) {
  RTC_LOG(LS_WARNING)
      << "Requested platform " << component << " processing, but " << reason
      << "; "
         "software fallback is disabled by platform mode.";
}

bool DisablePlatformEffect(AudioDeviceModule* adm,
                           AvailabilityFn is_available,
                           EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(false) == 0;
}

bool EnablePlatformEffect(AudioDeviceModule* adm,
                          AvailabilityFn is_available,
                          EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(true) == 0;
}

bool ApplyIndependentPlatformEffectAndResolveSoftware(std::optional<bool> enabled,
                                                      std::optional<AudioProcessingMode> mode,
                                                      const char *component, AudioDeviceModule *adm,
                                                      AvailabilityFn is_available,
                                                      EnableFn enable) {
  if (!enabled.has_value()) {
    return false;
  }
  if (!*enabled) {
    DisablePlatformEffect(adm, is_available, enable);
    return false;
  }

  switch (AudioProcessingModeOrAutomatic(mode)) {
    case AudioProcessingMode::kAutomatic:
      return !EnablePlatformEffect(adm, is_available, enable);
    case AudioProcessingMode::kPlatform: {
      const bool platform_enabled =
          EnablePlatformEffect(adm, is_available, enable);
      if (!platform_enabled) {
        LogPlatformOnlyRequestDisabled(
            component, "platform processing could not be enabled");
      }
      return false;
    }
    case AudioProcessingMode::kSoftware:
      DisablePlatformEffect(adm, is_available, enable);
      return true;
  }
  return false;
}

bool PlatformEffectIsAvailable(AudioDeviceModule* adm,
                               AvailabilityFn is_available) {
  return adm != nullptr && (adm->*is_available)();
}

bool SetPlatformEffect(AudioDeviceModule* adm, EnableFn enable, bool enabled) {
  return adm != nullptr && (adm->*enable)(enabled) == 0;
}

bool CoupledEchoNoisePlatformPathIsActive(AudioDeviceModule *adm) {
  if (adm == nullptr) {
    return false;
  }

  AudioDeviceModule::BuiltInAudioProcessingState state = adm->GetBuiltInAudioProcessingState();
  if (state.is_echo_cancellation_observed.has_value() ||
      state.is_noise_suppression_observed.has_value()) {
    return state.is_echo_cancellation_observed.value_or(false) ||
           state.is_noise_suppression_observed.value_or(false);
  }

  return (state.is_echo_cancellation_available || state.is_noise_suppression_available) &&
         (state.is_echo_cancellation_requested.value_or(false) ||
          state.is_noise_suppression_requested.value_or(false));
}

bool BuiltInVoiceProcessingPathIsAvailable(AudioDeviceModule *adm) {
  return adm != nullptr && adm->BuiltInVoiceProcessingPathIsAvailable();
}

bool SetBuiltInVoiceProcessingPath(AudioDeviceModule *adm, bool enabled) {
  return adm != nullptr && adm->EnableBuiltInVoiceProcessingPath(enabled) == 0;
}

std::optional<bool> ResolveCoupledSoftwareAndWarnPlatformOnlyDisabled(
    std::optional<bool> enabled, std::optional<AudioProcessingMode> mode, bool platform_enabled,
    const char *component) {
  std::optional<bool> software_enabled =
      ResolveAudioProcessingSoftwareFromPlatformState(enabled, mode, platform_enabled);
  if (AudioProcessingOptionIsPlatformOnly(enabled, mode) && !platform_enabled) {
    LogPlatformOnlyRequestDisabled(component,
                                   "the coupled platform processing path is "
                                   "disabled");
  }
  return software_enabled;
}

bool ResolveHighPassFilter(std::optional<bool> enabled,
                           std::optional<AudioProcessingMode> mode) {
  // No supported platform HPF exists today. Keep `platform` strict and disabled
  // instead of silently falling back to software.
  if (!enabled.has_value() || !*enabled) {
    return false;
  }
  if (AudioProcessingOptionIsPlatformOnly(enabled, mode)) {
    LogPlatformOnlyRequestDisabled("high-pass filter",
                                   "no platform high-pass filter is available");
  }
  return AudioProcessingModeOrAutomatic(mode) != AudioProcessingMode::kPlatform;
}

AudioProcessingImplementation ResolveEffectiveImplementation(
    std::optional<bool> is_software_enabled, bool is_platform_available,
    std::optional<bool> is_platform_requested, std::optional<bool> is_platform_observed) {
  const bool software_active = is_software_enabled.value_or(false);
  const bool platform_active =
      is_platform_observed.value_or(is_platform_available && is_platform_requested.value_or(false));

  if (software_active && platform_active) {
    return AudioProcessingImplementation::kSoftwareAndPlatform;
  }
  if (software_active) {
    return AudioProcessingImplementation::kSoftware;
  }
  if (platform_active) {
    return AudioProcessingImplementation::kPlatform;
  }
  if (is_software_enabled.has_value() || is_platform_requested.has_value() ||
      is_platform_observed.has_value() || is_platform_available) {
    return AudioProcessingImplementation::kDisabled;
  }
  return AudioProcessingImplementation::kUnknown;
}

AudioProcessingComponentRuntimeState BuildComponentRuntimeState(
    std::optional<bool> is_requested_enabled, std::optional<AudioProcessingMode> requested_mode,
    std::optional<bool> is_software_enabled, bool is_platform_available,
    std::optional<bool> is_platform_requested, std::optional<bool> is_platform_observed) {
  AudioProcessingComponentRuntimeState state;
  state.is_requested_enabled = is_requested_enabled;
  state.requested_mode = requested_mode;
  state.is_software_enabled = is_software_enabled;
  state.is_platform_available = is_platform_available;
  state.is_platform_requested = is_platform_requested;
  state.is_platform_observed = is_platform_observed;
  state.effective =
      ResolveEffectiveImplementation(state.is_software_enabled, state.is_platform_available,
                                     state.is_platform_requested, state.is_platform_observed);
  return state;
}

AudioOptions ApplyCoupledEchoNoiseProcessingOptions(
    AudioDeviceModule* adm,
    const AudioOptions& options_in) {
  // Apple Voice Processing I/O exposes AEC and NS through one shared path.
  // Bypassing that path at runtime can leave CoreAudio processing through a
  // VPIO-shaped route while WebRTC APM is also active. For coupled topologies,
  // software or disabled AEC/NS removes the built-in path entirely. Platform
  // AEC/NS recreates the path and then enables both shared effects.
  //
  // A single AEC or NS automatic/platform request can turn the shared path on
  // for both effects. Disabling the sibling component does not veto that
  // request because Apple cannot provide platform AEC without the shared path.
  // A software request still keeps the shared path off to avoid mixing Apple's
  // coupled processing with WebRTC APM. AGC is resolved after that decision and
  // never turns VPIO on by itself. This keeps partial AGC-only updates from
  // changing Apple audio routing.
  AudioOptions software_options = options_in;

  CoupledAudioProcessingPathResolution path_resolution = ResolveCoupledAudioProcessingPath(
      options_in, [adm] { return CoupledEchoNoisePlatformPathIsActive(adm); });
  bool vpio_enabled = false;

  if (path_resolution.should_update_echo_noise_platform_path) {
    const bool path_available = BuiltInVoiceProcessingPathIsAvailable(adm);
    const bool should_enable_vpio =
        path_available && path_resolution.should_use_echo_noise_platform_path;

    if (should_enable_vpio) {
      const bool path_enabled = SetBuiltInVoiceProcessingPath(adm, true);
      const bool effects_available =
          path_enabled &&
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
          SetBuiltInVoiceProcessingPath(adm, false);
        }
      }
    } else if (path_available) {
      SetBuiltInVoiceProcessingPath(adm, false);
    }
  } else {
    vpio_enabled = path_resolution.should_use_echo_noise_platform_path;
  }

  if (options_in.echo_cancellation.has_value()) {
    software_options.echo_cancellation = ResolveCoupledSoftwareAndWarnPlatformOnlyDisabled(
        options_in.echo_cancellation, options_in.echo_cancellation_mode, vpio_enabled,
        "echo cancellation");
  }

  if (options_in.noise_suppression.has_value()) {
    software_options.noise_suppression = ResolveCoupledSoftwareAndWarnPlatformOnlyDisabled(
        options_in.noise_suppression, options_in.noise_suppression_mode, vpio_enabled,
        "noise suppression");
  }

  if (options_in.auto_gain_control.has_value()) {
    // Apple AGC has its own switch, but it only has effect while the shared
    // AEC/NS VPIO path is active. AGC alone never enables VPIO, so `auto`
    // falls back to software when AEC/NS did not enable the shared path.
    const bool agc_switch_available =
        PlatformEffectIsAvailable(adm, &AudioDeviceModule::BuiltInAGCIsAvailable);
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
    software_options.auto_gain_control = ResolveCoupledSoftwareAndWarnPlatformOnlyDisabled(
        options_in.auto_gain_control, options_in.auto_gain_control_mode, agc_platform_enabled,
        "auto gain control");
  }

  return software_options;
}

std::optional<AudioProcessing::Config> GetApmConfig(AudioProcessing *apm) {
  if (apm == nullptr) {
    return std::nullopt;
  }
  return apm->GetConfig();
}

}  // namespace

AudioOptions ApplyAudioProcessingOptions(AudioProcessing* apm,
                                         AudioDeviceModule* adm,
                                         const AudioOptions& options_in) {
  AudioOptions software_options = options_in;

  if (adm != nullptr &&
      adm->GetBuiltInAudioProcessingTopology() ==
          AudioDeviceModule::BuiltInAudioProcessingTopology::
              kEchoCancellationAndNoiseSuppressionCoupled) {
    software_options =
        ApplyCoupledEchoNoiseProcessingOptions(adm, options_in);
  } else {
    if (options_in.echo_cancellation.has_value()) {
      software_options.echo_cancellation = ApplyIndependentPlatformEffectAndResolveSoftware(
          options_in.echo_cancellation, options_in.echo_cancellation_mode, "echo cancellation", adm,
          &AudioDeviceModule::BuiltInAECIsAvailable, &AudioDeviceModule::EnableBuiltInAEC);
    }

    if (options_in.auto_gain_control.has_value()) {
      software_options.auto_gain_control = ApplyIndependentPlatformEffectAndResolveSoftware(
          options_in.auto_gain_control, options_in.auto_gain_control_mode, "auto gain control", adm,
          &AudioDeviceModule::BuiltInAGCIsAvailable, &AudioDeviceModule::EnableBuiltInAGC);
    }

    if (options_in.noise_suppression.has_value()) {
      software_options.noise_suppression = ApplyIndependentPlatformEffectAndResolveSoftware(
          options_in.noise_suppression, options_in.noise_suppression_mode, "noise suppression", adm,
          &AudioDeviceModule::BuiltInNSIsAvailable, &AudioDeviceModule::EnableBuiltInNS);
    }
  }

  if (options_in.highpass_filter.has_value()) {
    software_options.highpass_filter = ResolveHighPassFilter(
        options_in.highpass_filter, options_in.highpass_filter_mode);
  }

  if (apm == nullptr) {
    return software_options;
  }

  AudioProcessing::Config apm_config = apm->GetConfig();

  if (software_options.echo_cancellation.has_value()) {
    apm_config.echo_canceller.enabled = *software_options.echo_cancellation;
  }

  if (software_options.auto_gain_control.has_value()) {
    const bool enabled = *software_options.auto_gain_control;
    apm_config.gain_controller1.enabled = enabled;
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC) || defined(WEBRTC_ANDROID)
    apm_config.gain_controller1.mode =
        AudioProcessing::Config::GainController1::kFixedDigital;
#else
    apm_config.gain_controller1.mode =
        AudioProcessing::Config::GainController1::kAdaptiveAnalog;
#endif
  }

  if (software_options.highpass_filter.has_value()) {
    apm_config.high_pass_filter.enabled = *software_options.highpass_filter;
  }

  if (software_options.noise_suppression.has_value()) {
    const bool enabled = *software_options.noise_suppression;
    apm_config.noise_suppression.enabled = enabled;
    apm_config.noise_suppression.level =
        AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  }

  apm->ApplyConfig(apm_config);
  return software_options;
}

AudioProcessingRuntimeState GetAudioProcessingRuntimeState(AudioProcessing *apm,
                                                           AudioDeviceModule *adm,
                                                           const AudioOptions &requested_options) {
  AudioDeviceModule::BuiltInAudioProcessingState built_in;
  if (adm != nullptr) {
    built_in = adm->GetBuiltInAudioProcessingState();
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
    software_auto_gain_control =
        apm_config->gain_controller1.enabled || apm_config->gain_controller2.enabled;
    software_high_pass_filter = apm_config->high_pass_filter.enabled;
  }

  AudioProcessingRuntimeState state;
  state.topology = built_in.topology;
  state.built_in = built_in;
  state.echo_cancellation = BuildComponentRuntimeState(
      requested_options.echo_cancellation, requested_options.echo_cancellation_mode,
      software_echo_cancellation, built_in.is_echo_cancellation_available,
      built_in.is_echo_cancellation_requested, built_in.is_echo_cancellation_observed);
  state.noise_suppression = BuildComponentRuntimeState(
      requested_options.noise_suppression, requested_options.noise_suppression_mode,
      software_noise_suppression, built_in.is_noise_suppression_available,
      built_in.is_noise_suppression_requested, built_in.is_noise_suppression_observed);
  state.auto_gain_control = BuildComponentRuntimeState(
      requested_options.auto_gain_control, requested_options.auto_gain_control_mode,
      software_auto_gain_control, built_in.is_auto_gain_control_available,
      built_in.is_auto_gain_control_requested, built_in.is_auto_gain_control_observed);
  state.high_pass_filter = BuildComponentRuntimeState(
      requested_options.highpass_filter, requested_options.highpass_filter_mode,
      software_high_pass_filter, false, std::nullopt, std::nullopt);
  return state;
}

}  // namespace webrtc
