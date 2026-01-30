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

#include "sdk/objc/native/src/push_audio_source.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<PushAudioSource> PushAudioSource::Create(
    int /*sample_rate*/,
    int /*channels*/) {
  // Note: sample_rate and channels parameters are kept for API compatibility
  // but not stored, as the actual format is determined by each PushData call.
  return rtc::make_ref_counted<PushAudioSource>();
}

PushAudioSource::PushAudioSource() = default;

void PushAudioSource::RegisterObserver(ObserverInterface* /*observer*/) {
  // State is always kLive - no need to notify observers
}

void PushAudioSource::UnregisterObserver(ObserverInterface* /*observer*/) {
  // State is always kLive - no need to notify observers
}

void PushAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(sink);
  MutexLock lock(&sink_lock_);
  RTC_DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
  RTC_LOG(LS_INFO) << "PushAudioSource::AddSink - total sinks: " << sinks_.size();
}

void PushAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(sink);
  MutexLock lock(&sink_lock_);
  sinks_.remove(sink);
}

void PushAudioSource::PushData(const void* audio_data,
                                int bits_per_sample,
                                int sample_rate,
                                size_t number_of_channels,
                                size_t number_of_frames) {
  MutexLock lock(&sink_lock_);
  static int push_count = 0;
  push_count++;
  if (push_count <= 5 || push_count % 100 == 0) {
    RTC_LOG(LS_INFO) << "PushAudioSource::PushData #" << push_count
                     << " - sinks: " << sinks_.size()
                     << ", channels: " << number_of_channels
                     << ", frames: " << number_of_frames
                     << ", rate: " << sample_rate;
  }
  for (auto* sink : sinks_) {
    // Pass audio data to each sink with no capture timestamp
    // (app audio doesn't have a meaningful capture time)
    sink->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                 number_of_frames, /*absolute_capture_timestamp_ms=*/std::nullopt);
  }
}

}  // namespace webrtc
