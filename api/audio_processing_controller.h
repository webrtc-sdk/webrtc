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

#include "api/audio/audio_device.h"
#include "api/audio/audio_processing.h"
#include "api/audio_options.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

bool RTC_EXPORT IsAudioProcessingModeValid(AudioProcessingMode mode);
bool RTC_EXPORT IsSystemAudioProcessingAvailable(AudioDeviceModule* adm);
bool RTC_EXPORT ShouldUseSystemAudioProcessing(AudioDeviceModule* adm,
                                               AudioProcessingMode mode);
void RTC_EXPORT ApplyAudioProcessingConfig(AudioProcessing* apm,
                                           const AudioOptions& options);
AudioOptions RTC_EXPORT ApplyAudioProcessingOptions(
    AudioProcessing* apm,
    AudioDeviceModule* adm,
    AudioProcessingMode mode,
    const AudioOptions& options);

}  // namespace webrtc

#endif  // API_AUDIO_PROCESSING_CONTROLLER_H_
