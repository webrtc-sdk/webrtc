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

#ifndef API_AUDIO_PROCESSING_CONTROLLER_H_
#define API_AUDIO_PROCESSING_CONTROLLER_H_

#include <optional>

#include "api/audio/audio_device.h"
#include "api/audio/audio_processing.h"
#include "api/audio_options.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class AudioProcessingMode {
  kAutomatic = 0,
  kSystem = 1,
  kSoftware = 2,
  kDisabled = 3,
};

enum class AudioProcessingLifecycle {
  kIdle = 0,
  kRunning = 1,
  kTransitioning = 2,
  kFailed = 3,
};

enum class AudioProcessingBackend {
  kDisabled = 0,
  kSystem = 1,
  kSoftware = 2,
  kUnavailable = 3,
};

struct RTC_EXPORT AudioProcessingState {
  AudioProcessingMode requested_mode = AudioProcessingMode::kAutomatic;
  AudioProcessingLifecycle lifecycle = AudioProcessingLifecycle::kIdle;
  AudioProcessingBackend backend = AudioProcessingBackend::kDisabled;
  AudioProcessingMode transition_from = AudioProcessingMode::kAutomatic;
  AudioProcessingMode transition_to = AudioProcessingMode::kAutomatic;
  int32_t last_error = 0;

  bool system_bypassed = false;
  bool system_agc_enabled = true;

  bool software_echo_cancellation = false;
  bool software_noise_suppression = false;
  bool software_auto_gain_control = false;
  bool software_highpass_filter = false;
};

class RTC_EXPORT AudioProcessingController {
 public:
  static AudioProcessingController& Shared();

  AudioProcessingMode requested_mode() const;
  AudioProcessingState state() const;

  void BeginTransition(AudioProcessingMode to);
  void CompleteTransition(int32_t result, bool running);

  void SetRequestedMode(AudioProcessingMode mode);
  void SetSystemPreferences(bool bypassed, bool agc_enabled);

  bool IsSystemProcessingAvailable(AudioDeviceModule* adm) const;
  bool ShouldEnableSystemProcessing(AudioDeviceModule* adm,
                                    AudioProcessingMode mode) const;

  AudioOptions ApplyOptions(AudioProcessing* apm,
                            AudioDeviceModule* adm,
                            const AudioOptions& options);
  void ReapplyLatestOptions();

 private:
  AudioProcessingController() = default;

  AudioOptions ApplyOptionsLocked(AudioProcessing* apm,
                                  AudioDeviceModule* adm,
                                  const AudioOptions& options);
  void UpdateLifecycleAndBackendLocked(AudioDeviceModule* adm,
                                       AudioProcessingBackend backend,
                                       int32_t error);

  mutable Mutex mutex_;
  AudioProcessingState state_ RTC_GUARDED_BY(mutex_);
  std::optional<AudioOptions> latest_options_ RTC_GUARDED_BY(mutex_);
  AudioProcessing* apm_ RTC_GUARDED_BY(mutex_) = nullptr;
  AudioDeviceModule* adm_ RTC_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace webrtc

#endif  // API_AUDIO_PROCESSING_CONTROLLER_H_
