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

#ifndef API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESULT_H_
#define API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESULT_H_

#include <string>
#include <utility>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Keep numeric values in sync with the Java and ObjC API enums because bridges
// pass these values across language boundaries.
enum class AudioProcessingOptionsResultCode {
  // Options were applied immediately by the component handling the request.
  kApplied = 0,
  // Options were accepted and stored. Active senders may reapply them later.
  kStored = 1,
  kRejectedRemoteTrack = 2,
  kRejectedInvalidCombination = 3,
  kRejectedPlatformUnavailable = 4,
  kApplyFailed = 5,
};

struct RTC_EXPORT AudioProcessingOptionsResult {
  AudioProcessingOptionsResultCode code = AudioProcessingOptionsResultCode::kApplied;
  std::string message;

  bool ok() const {
    return code == AudioProcessingOptionsResultCode::kApplied || code == AudioProcessingOptionsResultCode::kStored;
  }

  static AudioProcessingOptionsResult Applied() { return {AudioProcessingOptionsResultCode::kApplied, ""}; }

  static AudioProcessingOptionsResult Stored() { return {AudioProcessingOptionsResultCode::kStored, ""}; }

  static AudioProcessingOptionsResult Rejected(AudioProcessingOptionsResultCode code, std::string message) {
    return {code, std::move(message)};
  }
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESULT_H_
