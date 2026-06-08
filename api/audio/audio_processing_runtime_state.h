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

#ifndef API_AUDIO_AUDIO_PROCESSING_RUNTIME_STATE_H_
#define API_AUDIO_AUDIO_PROCESSING_RUNTIME_STATE_H_

#include <optional>

#include "api/audio/audio_device.h"
#include "api/audio_options.h"

namespace webrtc {

// Keep numeric values in sync with the Java and ObjC API enums because
// diagnostics pass these values across language boundaries.
enum class AudioProcessingImplementation {
  kUnknown = 0,
  kDisabled = 1,
  kSoftware = 2,
  kPlatform = 3,
  kSoftwareAndPlatform = 4,
};

struct AudioProcessingComponentRuntimeState {
  // The options most recently requested by the caller.
  std::optional<bool> is_requested_enabled;
  std::optional<AudioProcessingMode> requested_mode;

  // The software state selected by the platform/software resolver before it is
  // applied to APM. This lets diagnostics distinguish "ApplyOptions never ran"
  // from "ApplyOptions ran and selected disabled/software".
  std::optional<bool> is_resolved_software_enabled;

  // The current WebRTC APM state from AudioProcessing::GetConfig().
  std::optional<bool> is_software_enabled;
  // Platform state from the ADM. Active state wins over requested state when
  // deriving the effective implementation.
  bool is_platform_available = false;
  std::optional<bool> is_platform_requested;
  std::optional<bool> is_platform_active;

  AudioProcessingImplementation effective = AudioProcessingImplementation::kUnknown;
};

struct AudioProcessingRuntimeState {
  AudioDeviceModule::BuiltInAudioProcessingTopology topology =
      AudioDeviceModule::BuiltInAudioProcessingTopology::kIndependent;

  bool has_audio_processing_module = false;
  bool has_audio_processing_config = false;
  bool has_requested_audio_processing_options = false;
  bool has_resolved_audio_processing_options = false;

  AudioProcessingComponentRuntimeState echo_cancellation;
  AudioProcessingComponentRuntimeState noise_suppression;
  AudioProcessingComponentRuntimeState auto_gain_control;
  AudioProcessingComponentRuntimeState high_pass_filter;

  AudioDeviceModule::BuiltInAudioProcessingState built_in;
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_PROCESSING_RUNTIME_STATE_H_
