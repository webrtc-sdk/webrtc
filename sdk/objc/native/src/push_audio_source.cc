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

#include <algorithm>

#include "api/audio_options.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<PushAudioSource> PushAudioSource::Create(
    int /*sample_rate*/,
    int /*channels*/) {
  return rtc::make_ref_counted<PushAudioSource>();
}

PushAudioSource::PushAudioSource() = default;
PushAudioSource::~PushAudioSource() = default;

void PushAudioSource::RegisterObserver(ObserverInterface* /*observer*/) {}
void PushAudioSource::UnregisterObserver(ObserverInterface* /*observer*/) {}

void PushAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(sink);
  MutexLock lock(&sink_lock_);
  sinks_.push_back(sink);
  RTC_LOG(LS_INFO) << "PushAudioSource::AddSink - total sinks: " << sinks_.size();
}

void PushAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(sink);
  MutexLock lock(&sink_lock_);
  sinks_.remove(sink);
}

const AudioOptions PushAudioSource::options() const {
  AudioOptions options;
  options.bypass_adm = true;
  return options;
}

void PushAudioSource::PushData(const void* audio_data,
                                int bits_per_sample,
                                int sample_rate,
                                size_t number_of_channels,
                                size_t number_of_frames) {
  push_count_++;

  size_t num_sinks = 0;
  // Forward to all sinks. In a standard PeerConnection setup, one of these sinks
  // will be the LocalAudioSinkAdapter created by AudioRtpSender, which feeds
  // the encoding pipeline.
  {
    MutexLock lock(&sink_lock_);
    num_sinks = sinks_.size();
    for (auto* sink : sinks_) {
      sink->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                   number_of_frames, /*absolute_capture_timestamp_ms=*/std::nullopt);
    }
  }

  if (push_count_ <= 5 || push_count_ % 1000 == 0) {
    RTC_LOG(LS_INFO) << "PushAudioSource::PushData #" << push_count_
                     << " sinks=" << num_sinks
                     << " rate=" << sample_rate
                     << " channels=" << number_of_channels
                     << " frames=" << number_of_frames;
  }
}

}  // namespace webrtc
