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

#include "rtc_base/logging.h"

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

}  // namespace

AudioProcessingController& AudioProcessingController::Shared() {
  static AudioProcessingController* instance = new AudioProcessingController();
  return *instance;
}

AudioProcessingMode AudioProcessingController::requested_mode() const {
  MutexLock lock(&mutex_);
  return state_.requested_mode;
}

AudioProcessingState AudioProcessingController::state() const {
  MutexLock lock(&mutex_);
  return state_;
}

void AudioProcessingController::BeginTransition(AudioProcessingMode to) {
  MutexLock lock(&mutex_);
  state_.transition_from = state_.requested_mode;
  state_.transition_to = to;
  state_.lifecycle = AudioProcessingLifecycle::kTransitioning;
  state_.last_error = 0;
}

void AudioProcessingController::CompleteTransition(int32_t result,
                                                   bool running) {
  MutexLock lock(&mutex_);
  state_.last_error = result;
  state_.lifecycle = result == 0 ? (running ? AudioProcessingLifecycle::kRunning
                                            : AudioProcessingLifecycle::kIdle)
                                : AudioProcessingLifecycle::kFailed;
}

void AudioProcessingController::SetRequestedMode(AudioProcessingMode mode) {
  MutexLock lock(&mutex_);
  state_.requested_mode = mode;
}

void AudioProcessingController::SetSystemPreferences(bool bypassed,
                                                     bool agc_enabled) {
  MutexLock lock(&mutex_);
  state_.system_bypassed = bypassed;
  state_.system_agc_enabled = agc_enabled;
}

bool AudioProcessingController::IsSystemProcessingAvailable(
    AudioDeviceModule* adm) const {
  return adm != nullptr && adm->BuiltInAECIsAvailable();
}

bool AudioProcessingController::ShouldEnableSystemProcessing(
    AudioDeviceModule* adm,
    AudioProcessingMode mode) const {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
      return IsSystemProcessingAvailable(adm);
    case AudioProcessingMode::kSystem:
      return IsSystemProcessingAvailable(adm);
    case AudioProcessingMode::kSoftware:
    case AudioProcessingMode::kDisabled:
      return false;
  }
}

AudioOptions AudioProcessingController::ApplyOptions(
    AudioProcessing* apm,
    AudioDeviceModule* adm,
    const AudioOptions& options) {
  MutexLock lock(&mutex_);
  apm_ = apm;
  adm_ = adm;
  latest_options_ = options;
  return ApplyOptionsLocked(apm, adm, options);
}

void AudioProcessingController::ReapplyLatestOptions() {
  MutexLock lock(&mutex_);
  if (!latest_options_) {
    return;
  }

  ApplyOptionsLocked(apm_, adm_, *latest_options_);
}

AudioOptions AudioProcessingController::ApplyOptionsLocked(
    AudioProcessing* apm,
    AudioDeviceModule* adm,
    const AudioOptions& options_in) {
  AudioOptions options = options_in;
  AudioProcessingBackend backend = AudioProcessingBackend::kDisabled;
  int32_t error = 0;

  switch (state_.requested_mode) {
    case AudioProcessingMode::kDisabled:
      ForceSoftwareProcessing(&options, false);
      backend = AudioProcessingBackend::kDisabled;
      break;

    case AudioProcessingMode::kSoftware:
      backend = AnySoftwareProcessingEnabled(options)
                    ? AudioProcessingBackend::kSoftware
                    : AudioProcessingBackend::kDisabled;
      break;

    case AudioProcessingMode::kSystem:
      if (!IsSystemProcessingAvailable(adm)) {
        ForceSoftwareProcessing(&options, false);
        backend = AudioProcessingBackend::kUnavailable;
        error = -1;
      } else {
        ForceSoftwareProcessing(&options, false);
        backend = AudioProcessingBackend::kSystem;
      }
      break;

    case AudioProcessingMode::kAutomatic:
      if (IsSystemProcessingAvailable(adm)) {
        ForceSoftwareProcessing(&options, false);
        backend = AudioProcessingBackend::kSystem;
      } else {
        backend = AnySoftwareProcessingEnabled(options)
                      ? AudioProcessingBackend::kSoftware
                      : AudioProcessingBackend::kDisabled;
      }
      break;
  }

  ApplyAudioProcessingConfig(apm, options);

  state_.software_echo_cancellation = options.echo_cancellation.value_or(false);
  state_.software_auto_gain_control = options.auto_gain_control.value_or(false);
  state_.software_noise_suppression = options.noise_suppression.value_or(false);
  state_.software_highpass_filter = options.highpass_filter.value_or(false);
  UpdateLifecycleAndBackendLocked(adm, backend, error);

  RTC_LOG(LS_INFO) << "AudioProcessingController::ApplyOptions mode="
                   << static_cast<int>(state_.requested_mode)
                   << " backend=" << static_cast<int>(backend)
                   << " options=" << options.ToString();

  return options;
}

void AudioProcessingController::UpdateLifecycleAndBackendLocked(
    AudioDeviceModule* adm,
    AudioProcessingBackend backend,
    int32_t error) {
  state_.backend = backend;
  state_.last_error = error;

  if (state_.lifecycle == AudioProcessingLifecycle::kTransitioning) {
    return;
  }

  if (error != 0) {
    state_.lifecycle = AudioProcessingLifecycle::kFailed;
    return;
  }

  const bool running = adm != nullptr && (adm->Playing() || adm->Recording());
  state_.lifecycle = running ? AudioProcessingLifecycle::kRunning
                             : AudioProcessingLifecycle::kIdle;
}

}  // namespace webrtc
