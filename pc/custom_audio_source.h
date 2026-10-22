/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CUSTOM_AUDIO_SOURCE_H_
#define PC_CUSTOM_AUDIO_SOURCE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "pc/local_audio_source.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// An audio source that an application pushes PCM frames into, bypassing the
// AudioDeviceModule. Pushed frames are delivered to the source's sinks, which
// feed the attached AudioSendStreams directly. Multiple instances operate
// independently of each other and of the ADM capture path, and
// reporting SourceType::kCustom keeps AudioState from also pushing device
// audio into streams fed by this source.
//
// NOTE: capture-side APM (AEC/NS/AGC/HPF) is intentionally bypassed, since
// that processing runs in AudioTransportImpl on the ADM path only. Callers
// must supply audio that needs no such processing (e.g. app or file audio).
// Frames are still resampled to the encoder rate downstream in ChannelSend.
class CustomAudioSource : public LocalAudioSource {
 public:
  // queue_size_ms == 0: synchronous mode. PushFrame() delivers to sinks on
  //   the calling thread and frames must be exactly 10 ms. The caller must
  //   push from a single thread (or externally serialized) per source.
  // queue_size_ms > 0 (must be a multiple of 10): buffered mode. Frames are
  //   queued and a dedicated task queue paces 10 ms deliveries, feeding
  //   silence on underrun. task_queue_factory is required in this mode.
  // clock is used for pacing delay compensation. Passing nullptr selects
  // the real-time clock (tests inject a simulated one).
  // Returns nullptr for invalid arguments.
  static scoped_refptr<CustomAudioSource> Create(
      int sample_rate_hz,
      size_t num_channels,
      int queue_size_ms,
      TaskQueueFactory* task_queue_factory,
      const AudioOptions* options = nullptr,
      Clock* clock = nullptr);

  // AudioSourceInterface overrides. Unlike LocalAudioSource, a real sink
  // list is maintained and fed. Sinks must not re-enter AddSink/RemoveSink
  // from OnData. RemoveSink blocks until any in-flight delivery to the sink
  // has finished, so the sink may be destroyed once it returns.
  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;
  SourceType source_type() const override { return SourceType::kCustom; }

  // Pushes interleaved int16 PCM. sample_rate_hz and num_channels must match
  // the construction format in both modes.
  //
  // Returns false and does nothing if the format is wrong, if a synchronous
  // mode frame is not exactly 10 ms, if the buffer lacks room, or if a
  // previous completion callback is still pending.
  //
  // on_complete is the buffered-mode back-pressure signal: it fires once the
  // buffer has drained to the point where pushing again is reasonable. It is
  // invoked inline (before PushFrame returns) when the buffer is below that
  // threshold, otherwise later on the internal pacing thread. At most one
  // completion may be pending at a time. In synchronous mode it always fires
  // inline. It runs outside the source's lock, so pushing the next frame
  // from inside the handler is allowed. It must not block.
  bool PushFrame(const int16_t* data,
                 int sample_rate_hz,
                 size_t num_channels,
                 size_t samples_per_channel,
                 absl::AnyInvocable<void() &&> on_complete = nullptr);

  // Drops all queued audio (buffered mode). A pending completion fires.
  void ClearBuffer();

  // Currently queued audio in milliseconds (buffered mode, 0 otherwise).
  int64_t BufferedDurationMs() const;

  int sample_rate_hz() const { return sample_rate_hz_; }
  size_t num_channels() const { return num_channels_; }
  int queue_size_ms() const { return queue_size_ms_; }

 protected:
  CustomAudioSource(int sample_rate_hz,
                    size_t num_channels,
                    int queue_size_ms,
                    TaskQueueFactory* task_queue_factory,
                    Clock* clock);
  ~CustomAudioSource() override;

 private:
  // Delivers one frame to the given sinks. Synchronous mode calls it under
  // mutex_ (serializing concurrent pushers), the pacer calls it outside the
  // lock with a snapshot of sinks_ and pacer-owned frame data.
  void DeliverToSinks(const std::vector<AudioTrackSinkInterface*>& sinks,
                      const int16_t* data,
                      size_t samples_per_channel) const;
  // Returns the pending completion if the buffer drained below the
  // threshold, to be invoked after releasing mutex_.
  absl::AnyInvocable<void() &&> TakeCompletionIfDue()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const int sample_rate_hz_;
  const size_t num_channels_;
  const int queue_size_ms_;
  // Samples (all channels) per 10 ms frame.
  const size_t samples_per_10ms_;
  // 0 in synchronous mode. Buffer capacity is queue_size_samples_ +
  // notify_threshold_samples_ (2x the queue size).
  size_t queue_size_samples_ = 0;
  size_t notify_threshold_samples_ = 0;

  mutable Mutex mutex_;
  // Held by the pacer across sink snapshot and fan-out. RemoveSink acquires
  // it (after releasing mutex_) as a fence so no OnData can reach a removed
  // sink once RemoveSink returns. Never acquired while holding mutex_.
  mutable Mutex delivery_mutex_;
  std::vector<AudioTrackSinkInterface*> sinks_ RTC_GUARDED_BY(mutex_);
  // FIFO of pushed samples. Samples before read_offset_ are already
  // consumed and reclaimed lazily by the pacer, keeping the per-tick
  // dequeue amortized O(frame) instead of memmoving the remainder.
  std::vector<int16_t> buffer_ RTC_GUARDED_BY(mutex_);
  size_t read_offset_ RTC_GUARDED_BY(mutex_) = 0;
  absl::AnyInvocable<void() &&> on_complete_ RTC_GUARDED_BY(mutex_);
  // One-frame scratch the pacer fills under mutex_ and delivers outside
  // the lock. Only the pacer thread touches it.
  std::vector<int16_t> frame_buffer_;

  RepeatingTaskHandle audio_task_;
  // Destroyed first in ~CustomAudioSource (explicitly reset) so no pacer
  // tick can run while the members above are torn down.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> audio_queue_;
};

}  // namespace webrtc

#endif  // PC_CUSTOM_AUDIO_SOURCE_H_
