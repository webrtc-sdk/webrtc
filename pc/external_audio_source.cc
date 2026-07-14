/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/external_audio_source.h"

#include <algorithm>
#include <utility>

#include "api/make_ref_counted.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr int kBitsPerSample = 16;
constexpr int kFrameDurationMs = 10;
}  // namespace

scoped_refptr<ExternalAudioSource> ExternalAudioSource::Create(
    int sample_rate_hz,
    size_t num_channels,
    int queue_size_ms,
    TaskQueueFactory* task_queue_factory,
    const AudioOptions* options,
    Clock* clock) {
  // Validated here as the single point of truth so wrappers (e.g. the ObjC
  // factory method) don't need to duplicate the rules.
  if (sample_rate_hz <= 0 || sample_rate_hz % 100 != 0 || num_channels == 0 ||
      queue_size_ms < 0 || queue_size_ms % kFrameDurationMs != 0 ||
      (queue_size_ms > 0 && !task_queue_factory)) {
    RTC_LOG(LS_ERROR)
        << "ExternalAudioSource: invalid arguments (sample rate must be a "
        << "positive multiple of 100, channels > 0, queue size a "
        << "non-negative multiple of 10 ms, buffered mode requires a task "
        << "queue factory), got " << sample_rate_hz << "Hz/" << num_channels
        << "ch/" << queue_size_ms << "ms";
    return nullptr;
  }

  auto source = make_ref_counted<ExternalAudioSource>(
      sample_rate_hz, num_channels, queue_size_ms, task_queue_factory, clock);
  if (options) {
    source->SetOptions(*options);
  }
  return source;
}

ExternalAudioSource::ExternalAudioSource(int sample_rate_hz,
                                         size_t num_channels,
                                         int queue_size_ms,
                                         TaskQueueFactory* task_queue_factory,
                                         Clock* clock)
    : sample_rate_hz_(sample_rate_hz),
      num_channels_(num_channels),
      queue_size_ms_(queue_size_ms),
      samples_per_10ms_(sample_rate_hz / 100 * num_channels) {
  if (queue_size_ms == 0) {
    // Synchronous mode: PushFrame() delivers directly, no pacer.
    return;
  }

  queue_size_samples_ = queue_size_ms / kFrameDurationMs * samples_per_10ms_;
  notify_threshold_samples_ = queue_size_samples_;
  // Consumed samples are reclaimed lazily, so the vector can grow past the
  // logical capacity by up to one queue of already-read samples.
  buffer_.reserve(queue_size_samples_ + notify_threshold_samples_ +
                  queue_size_samples_);
  frame_buffer_.assign(samples_per_10ms_, 0);

  audio_queue_ = task_queue_factory->CreateTaskQueue(
      "ExternalAudioSource", TaskQueueFactory::Priority::NORMAL);

  audio_task_ = RepeatingTaskHandle::Start(
      audio_queue_.get(),
      [this]() {
        absl::AnyInvocable<void() &&> completion;
        {
          // Held across snapshot and fan-out so RemoveSink can fence on it.
          MutexLock delivery_lock(&delivery_mutex_);
          std::vector<AudioTrackSinkInterface*> sinks;
          {
            MutexLock lock(&mutex_);
            // Copy up to one frame into the pacer-owned scratch, padding
            // with silence on (partial) underrun. A residual smaller than
            // one frame is flushed rather than left stranded in the buffer.
            const size_t available = buffer_.size() - read_offset_;
            const size_t to_copy = std::min(available, samples_per_10ms_);
            std::copy_n(buffer_.begin() + read_offset_, to_copy,
                        frame_buffer_.begin());
            std::fill(frame_buffer_.begin() + to_copy, frame_buffer_.end(),
                      0);
            read_offset_ += to_copy;
            // Reclaim consumed samples in one amortized pass instead of
            // memmoving the whole remainder every tick.
            if (read_offset_ == buffer_.size() ||
                read_offset_ >= queue_size_samples_) {
              buffer_.erase(buffer_.begin(), buffer_.begin() + read_offset_);
              read_offset_ = 0;
            }
            completion = TakeCompletionIfDue();
            sinks = sinks_;
          }
          // Deliver outside mutex_ so pushers don't wait on the per-sink
          // work. Only the pacer thread reads frame_buffer_.
          DeliverToSinks(sinks, frame_buffer_.data(),
                         samples_per_10ms_ / num_channels_);
        }
        // Invoked outside both locks so the handler may push the next frame
        // or remove a sink.
        if (completion) {
          std::move(completion)();
        }
        return TimeDelta::Millis(kFrameDurationMs);
      },
      TaskQueueBase::DelayPrecision::kHigh,
      clock ? clock : Clock::GetRealTimeClock());
}

ExternalAudioSource::~ExternalAudioSource() {
  // Stop the pacer before members are torn down. TaskQueueDeleter blocks
  // until an in-flight tick has finished and drops the pending repeat.
  audio_queue_ = nullptr;

  // Don't strand a caller waiting on back-pressure.
  absl::AnyInvocable<void() &&> pending;
  {
    MutexLock lock(&mutex_);
    pending = std::move(on_complete_);
  }
  if (pending) {
    std::move(pending)();
  }
}

void ExternalAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(sink);
  MutexLock lock(&mutex_);
  RTC_DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
}

void ExternalAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  {
    MutexLock lock(&mutex_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink),
                 sinks_.end());
  }
  // Fence: the pacer delivers from a snapshot taken under mutex_ but calls
  // OnData outside it while holding delivery_mutex_. Waiting on that lock
  // here guarantees no in-flight fan-out still references the sink once
  // this returns, so the caller may destroy it. mutex_ must not be held
  // while acquiring delivery_mutex_ (the pacer takes them in the opposite
  // order).
  MutexLock fence(&delivery_mutex_);
}

bool ExternalAudioSource::PushFrame(
    const int16_t* data,
    int sample_rate_hz,
    size_t num_channels,
    size_t samples_per_channel,
    absl::AnyInvocable<void() &&> on_complete) {
  if (sample_rate_hz != sample_rate_hz_ || num_channels != num_channels_) {
    RTC_LOG(LS_WARNING) << "ExternalAudioSource: rejected frame with format "
                        << sample_rate_hz << "Hz/" << num_channels
                        << "ch, source is " << sample_rate_hz_ << "Hz/"
                        << num_channels_ << "ch";
    return false;
  }
  const size_t num_samples = samples_per_channel * num_channels;

  {
    MutexLock lock(&mutex_);

    if (queue_size_samples_ == 0) {
      // Synchronous mode: the frame must be exactly 10 ms.
      if (samples_per_channel * 100 != static_cast<size_t>(sample_rate_hz_)) {
        RTC_LOG(LS_WARNING)
            << "ExternalAudioSource: synchronous mode requires 10 ms frames, "
            << "got " << samples_per_channel << " samples per channel";
        return false;
      }
      DeliverToSinks(sinks_, data, samples_per_channel);
    } else {
      const size_t capacity = queue_size_samples_ + notify_threshold_samples_;
      const size_t buffered = buffer_.size() - read_offset_;
      if (capacity - buffered < num_samples) {
        return false;
      }
      if (on_complete_) {
        // Only one in-flight push may await completion.
        return false;
      }

      buffer_.insert(buffer_.end(), data, data + num_samples);

      if (on_complete && buffered + num_samples > notify_threshold_samples_) {
        // Defer until the pacer drains the buffer below the threshold.
        on_complete_ = std::move(on_complete);
      }
    }
  }
  // Invoked outside the lock so the handler may push the next frame.
  if (on_complete) {
    std::move(on_complete)();
  }
  return true;
}

void ExternalAudioSource::ClearBuffer() {
  absl::AnyInvocable<void() &&> completion;
  {
    MutexLock lock(&mutex_);
    buffer_.clear();
    read_offset_ = 0;
    completion = TakeCompletionIfDue();
  }
  if (completion) {
    std::move(completion)();
  }
}

int64_t ExternalAudioSource::BufferedDurationMs() const {
  MutexLock lock(&mutex_);
  const int64_t buffered = buffer_.size() - read_offset_;
  // Multiply before dividing so rates that are not a multiple of 1000
  // (e.g. 44100) don't accumulate truncation error.
  return buffered * 1000 /
         (static_cast<int64_t>(sample_rate_hz_) * num_channels_);
}

void ExternalAudioSource::DeliverToSinks(
    const std::vector<AudioTrackSinkInterface*>& sinks,
    const int16_t* data,
    size_t samples_per_channel) const {
  for (AudioTrackSinkInterface* sink : sinks) {
    sink->OnData(data, kBitsPerSample, sample_rate_hz_, num_channels_,
                 samples_per_channel);
  }
}

absl::AnyInvocable<void() &&> ExternalAudioSource::TakeCompletionIfDue() {
  if (on_complete_ &&
      buffer_.size() - read_offset_ <= notify_threshold_samples_) {
    return std::move(on_complete_);
  }
  return nullptr;
}

}  // namespace webrtc
