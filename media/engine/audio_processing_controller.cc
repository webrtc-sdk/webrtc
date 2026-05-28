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

namespace webrtc {
namespace {

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

bool EnablePlatformEffect(AudioDeviceModule* adm,
                          AvailabilityFn is_available,
                          EnableFn enable) {
  if (adm == nullptr || !(adm->*is_available)()) {
    return false;
  }
  return (adm->*enable)(true) == 0;
}

bool ResolveSoftwareProcessing(std::optional<bool> enabled,
                               std::optional<AudioProcessingMode> mode,
                               AudioDeviceModule* adm,
                               AvailabilityFn is_available,
                               EnableFn enable) {
  if (!enabled.has_value()) {
    return false;
  }
  if (!*enabled) {
    DisablePlatformEffect(adm, is_available, enable);
    return false;
  }

  switch (mode.value_or(AudioProcessingMode::kAutomatic)) {
    case AudioProcessingMode::kAutomatic:
      return !EnablePlatformEffect(adm, is_available, enable);
    case AudioProcessingMode::kPlatform:
      EnablePlatformEffect(adm, is_available, enable);
      return false;
    case AudioProcessingMode::kSoftware:
      DisablePlatformEffect(adm, is_available, enable);
      return true;
  }
  return false;
}

bool ResolveHighPassFilter(std::optional<bool> enabled,
                           std::optional<AudioProcessingMode> mode) {
  if (!enabled.has_value() || !*enabled) {
    return false;
  }
  return mode.value_or(AudioProcessingMode::kAutomatic) !=
         AudioProcessingMode::kPlatform;
}

}  // namespace

AudioOptions ApplyAudioProcessingOptions(AudioProcessing* apm,
                                         AudioDeviceModule* adm,
                                         const AudioOptions& options_in) {
  AudioOptions software_options = options_in;

  if (options_in.echo_cancellation.has_value()) {
    software_options.echo_cancellation = ResolveSoftwareProcessing(
        options_in.echo_cancellation, options_in.echo_cancellation_mode, adm,
        &AudioDeviceModule::BuiltInAECIsAvailable,
        &AudioDeviceModule::EnableBuiltInAEC);
  }

  if (options_in.auto_gain_control.has_value()) {
    software_options.auto_gain_control = ResolveSoftwareProcessing(
        options_in.auto_gain_control, options_in.auto_gain_control_mode, adm,
        &AudioDeviceModule::BuiltInAGCIsAvailable,
        &AudioDeviceModule::EnableBuiltInAGC);
  }

  if (options_in.noise_suppression.has_value()) {
    software_options.noise_suppression = ResolveSoftwareProcessing(
        options_in.noise_suppression, options_in.noise_suppression_mode, adm,
        &AudioDeviceModule::BuiltInNSIsAvailable,
        &AudioDeviceModule::EnableBuiltInNS);
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

}  // namespace webrtc
