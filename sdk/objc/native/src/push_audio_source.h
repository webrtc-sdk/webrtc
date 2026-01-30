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

#ifndef SDK_OBJC_NATIVE_SRC_PUSH_AUDIO_SOURCE_H_
#define SDK_OBJC_NATIVE_SRC_PUSH_AUDIO_SOURCE_H_

#include <atomic>
#include <list>
#include <optional>

#include "api/media_stream_interface.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// Audio source that allows pushing PCM data directly, bypassing ADM completely.
// Used for stereo app audio during screen sharing.
//
// This source implements AudioSourceInterface to provide:
// - Track creation compatibility
// - Local monitoring sinks (via AudioTrackSinkInterface)
// - Custom AudioOptions (to signal bypass_adm)
class PushAudioSource : public AudioSourceInterface {
 public:
  static rtc::scoped_refptr<PushAudioSource> Create(int sample_rate,
                                                     int channels);

  // NotifierInterface implementation (inherited via AudioSourceInterface)
  void RegisterObserver(ObserverInterface* observer) override;
  void UnregisterObserver(ObserverInterface* observer) override;

  // AudioSourceInterface implementation (for track creation)
  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }
  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;
  const AudioOptions options() const override;

  // Push PCM data directly. It will be forwarded to all sinks, including
  // the one created by AudioRtpSender to feed the encoding pipeline.
  // audio_data: Pointer to interleaved PCM samples
  // bits_per_sample: Typically 16
  // sample_rate: e.g., 48000
  // number_of_channels: 1 for mono, 2 for stereo
  // number_of_frames: Number of samples per channel
  void PushData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames);

 protected:
  PushAudioSource();
  ~PushAudioSource() override;

 private:
  // For sinks (AudioTrackSinkInterface)
  Mutex sink_lock_;
  std::list<AudioTrackSinkInterface*> sinks_ RTC_GUARDED_BY(sink_lock_);

  // Debug counters
  std::atomic<uint64_t> push_count_{0};
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_PUSH_AUDIO_SOURCE_H_
