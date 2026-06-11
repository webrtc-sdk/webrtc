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

#ifndef MEDIA_ENGINE_AUDIO_PROCESSING_CONTROLLER_H_
#define MEDIA_ENGINE_AUDIO_PROCESSING_CONTROLLER_H_

#include <optional>

#include "api/audio/audio_device.h"
#include "api/audio/audio_processing.h"
#include "api/audio/audio_processing_options_result.h"
#include "api/audio/audio_processing_state.h"
#include "api/audio_options.h"

namespace webrtc {

AudioProcessingOptionsResult ValidateAudioProcessingOptionsForApply(AudioDeviceModule *adm,
                                                                    const AudioOptions &options);

struct AudioProcessingApplyResult {
  // Non-fatal apply outcome for ADM platform effects and APM software config.
  // Public setters validate unsupported requests before send state is mutated.
  // Apply-time platform failures resolve the affected component to disabled and
  // report a warning so sender updates can continue.
  AudioProcessingOptionsResult result = AudioProcessingOptionsResult::Applied();
  // AudioOptions after platform/software resolution. These are the values written
  // to WebRTC APM.
  AudioOptions resolved_options;
};

AudioProcessingApplyResult ApplyAudioProcessingOptions(AudioProcessing *apm, AudioDeviceModule *adm,
                                                       const AudioOptions &options);

AudioProcessingState GetAudioProcessingState(AudioProcessing *apm, AudioDeviceModule *adm,
                                                           const std::optional<AudioOptions> &requested_options,
                                                           const std::optional<AudioOptions> &resolved_options);

}  // namespace webrtc

#endif  // MEDIA_ENGINE_AUDIO_PROCESSING_CONTROLLER_H_
