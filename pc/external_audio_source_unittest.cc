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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/local_audio_source.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

constexpr int kSampleRate = 48000;
constexpr size_t kNumChannels = 2;
constexpr size_t kSamplesPer10Ms = kSampleRate / 100 * kNumChannels;
constexpr int kQueueSizeMs = 40;

class FakeSink : public AudioTrackSinkInterface {
 public:
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override {
    EXPECT_EQ(bits_per_sample, 16);
    last_sample_rate_ = sample_rate;
    last_num_channels_ = number_of_channels;
    ++frames_received_;
    const int16_t* samples = static_cast<const int16_t*>(audio_data);
    received_.insert(received_.end(), samples,
                     samples + number_of_frames * number_of_channels);
  }

  int frames_received_ = 0;
  int last_sample_rate_ = 0;
  size_t last_num_channels_ = 0;
  std::vector<int16_t> received_;
};

std::vector<int16_t> RampFrame(size_t num_samples, int16_t start = 1) {
  std::vector<int16_t> data(num_samples);
  std::iota(data.begin(), data.end(), start);
  return data;
}

class ExternalAudioSourceTest : public ::testing::Test {
 protected:
  ExternalAudioSourceTest()
      : time_controller_(Timestamp::Seconds(1)),
        task_queue_factory_(time_controller_.CreateTaskQueueFactory()) {}

  scoped_refptr<ExternalAudioSource> CreateSource(int queue_size_ms) {
    return ExternalAudioSource::Create(
        kSampleRate, kNumChannels, queue_size_ms, task_queue_factory_.get(),
        /*options=*/nullptr, time_controller_.GetClock());
  }

  void AdvanceMs(int64_t ms) {
    time_controller_.AdvanceTime(TimeDelta::Millis(ms));
  }

  GlobalSimulatedTimeController time_controller_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
};

TEST_F(ExternalAudioSourceTest, CreateRejectsInvalidArguments) {
  EXPECT_EQ(ExternalAudioSource::Create(0, kNumChannels, 0,
                                        task_queue_factory_.get()),
            nullptr);
  EXPECT_EQ(ExternalAudioSource::Create(44110, kNumChannels, 0,
                                        task_queue_factory_.get()),
            nullptr);
  EXPECT_EQ(ExternalAudioSource::Create(kSampleRate, 0, 0,
                                        task_queue_factory_.get()),
            nullptr);
  EXPECT_EQ(ExternalAudioSource::Create(kSampleRate, kNumChannels, 15,
                                        task_queue_factory_.get()),
            nullptr);
  EXPECT_EQ(ExternalAudioSource::Create(kSampleRate, kNumChannels, 100,
                                        /*task_queue_factory=*/nullptr),
            nullptr);
}

TEST_F(ExternalAudioSourceTest, IsExternalSourceIsTrue) {
  auto source = CreateSource(/*queue_size_ms=*/0);
  EXPECT_TRUE(source->is_external_source());
  // The base local source (ADM path) must stay non-external.
  auto local = LocalAudioSource::Create(nullptr);
  EXPECT_FALSE(local->is_external_source());
}

TEST_F(ExternalAudioSourceTest, AppliesAudioOptions) {
  AudioOptions options;
  options.echo_cancellation = false;
  auto source = ExternalAudioSource::Create(
      kSampleRate, kNumChannels, /*queue_size_ms=*/0,
      task_queue_factory_.get(), &options, time_controller_.GetClock());
  EXPECT_EQ(source->options().echo_cancellation, false);
}

TEST_F(ExternalAudioSourceTest, SyncModeDeliversToAllSinks) {
  auto source = CreateSource(/*queue_size_ms=*/0);
  FakeSink sink1;
  FakeSink sink2;
  source->AddSink(&sink1);
  source->AddSink(&sink2);

  std::vector<int16_t> frame = RampFrame(kSamplesPer10Ms);
  bool completed = false;
  EXPECT_TRUE(source->PushFrame(frame.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms / kNumChannels,
                                [&] { completed = true; }));
  // Synchronous mode delivers and completes inline.
  EXPECT_TRUE(completed);
  EXPECT_EQ(sink1.frames_received_, 1);
  EXPECT_EQ(sink2.frames_received_, 1);
  EXPECT_EQ(sink1.received_, frame);
  EXPECT_EQ(sink1.last_sample_rate_, kSampleRate);
  EXPECT_EQ(sink1.last_num_channels_, kNumChannels);

  source->RemoveSink(&sink2);
  EXPECT_TRUE(source->PushFrame(frame.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms / kNumChannels));
  EXPECT_EQ(sink1.frames_received_, 2);
  EXPECT_EQ(sink2.frames_received_, 1);
}

TEST_F(ExternalAudioSourceTest, SyncModeRejectsNon10MsFrames) {
  auto source = CreateSource(/*queue_size_ms=*/0);
  FakeSink sink;
  source->AddSink(&sink);

  std::vector<int16_t> frame = RampFrame(kSamplesPer10Ms * 2);
  EXPECT_FALSE(source->PushFrame(frame.data(), kSampleRate, kNumChannels,
                                 /*samples_per_channel=*/kSamplesPer10Ms * 2 /
                                     kNumChannels));
  EXPECT_EQ(sink.frames_received_, 0);
}

TEST_F(ExternalAudioSourceTest, RejectsMismatchedFormat) {
  auto source = CreateSource(kQueueSizeMs);
  std::vector<int16_t> frame = RampFrame(kSamplesPer10Ms);

  EXPECT_FALSE(source->PushFrame(frame.data(), /*sample_rate_hz=*/44100,
                                 kNumChannels,
                                 kSamplesPer10Ms / kNumChannels));
  EXPECT_FALSE(source->PushFrame(frame.data(), kSampleRate,
                                 /*num_channels=*/1,
                                 kSamplesPer10Ms / kNumChannels));
  EXPECT_EQ(source->BufferedDurationMs(), 0);
}

TEST_F(ExternalAudioSourceTest, BufferedModePacesDeliveryIn10MsFrames) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  // Push 20 ms at once.
  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 2);
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 2 / kNumChannels));
  EXPECT_EQ(source->BufferedDurationMs(), 20);

  AdvanceMs(20);
  // Ticks may include the initial immediate run, but at least the two pushed
  // frames must be out by now, in order, split into 10 ms deliveries.
  ASSERT_GE(sink.frames_received_, 2);
  ASSERT_GE(sink.received_.size(), data.size());
  EXPECT_TRUE(
      std::equal(data.begin(), data.end(), sink.received_.begin()));
  EXPECT_EQ(source->BufferedDurationMs(), 0);
}

TEST_F(ExternalAudioSourceTest, BufferedModeFlushesSubFrameResidual) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  // Push 15 ms: one full frame plus a 5 ms residual.
  const size_t samples = kSamplesPer10Ms + kSamplesPer10Ms / 2;
  std::vector<int16_t> data = RampFrame(samples);
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                samples / kNumChannels));

  AdvanceMs(30);
  // All pushed samples must be delivered, the residual padded with silence.
  EXPECT_EQ(source->BufferedDurationMs(), 0);
  ASSERT_GE(sink.received_.size(), samples);
  EXPECT_TRUE(std::equal(data.begin(), data.end(), sink.received_.begin()));
  // The remainder of the residual frame is silence.
  for (size_t i = samples; i < kSamplesPer10Ms * 2; ++i) {
    ASSERT_EQ(sink.received_[i], 0);
  }
}

TEST_F(ExternalAudioSourceTest, BufferedModeFeedsSilenceOnUnderrun) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  AdvanceMs(30);
  // No audio was pushed, so cadence is kept with silence frames.
  ASSERT_GE(sink.frames_received_, 3);
  for (int16_t sample : sink.received_) {
    ASSERT_EQ(sample, 0);
  }
}

TEST_F(ExternalAudioSourceTest, BufferedDurationIsExactForNonMultipleRates) {
  // 44100 is valid (multiple of 100) but not a multiple of 1000.
  auto source = ExternalAudioSource::Create(
      44100, kNumChannels, /*queue_size_ms=*/500, task_queue_factory_.get(),
      /*options=*/nullptr, time_controller_.GetClock());

  // Exactly one second of audio.
  std::vector<int16_t> data = RampFrame(44100 * kNumChannels);
  EXPECT_TRUE(source->PushFrame(data.data(), 44100, kNumChannels, 44100));
  EXPECT_EQ(source->BufferedDurationMs(), 1000);
}

TEST_F(ExternalAudioSourceTest, BufferedModeRejectsWhenFull) {
  auto source = CreateSource(kQueueSizeMs);

  // Capacity is 2x the queue size (80 ms).
  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 8);
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 8 / kNumChannels));
  EXPECT_EQ(source->BufferedDurationMs(), 80);

  std::vector<int16_t> extra = RampFrame(kSamplesPer10Ms);
  EXPECT_FALSE(source->PushFrame(extra.data(), kSampleRate, kNumChannels,
                                 kSamplesPer10Ms / kNumChannels));
}

TEST_F(ExternalAudioSourceTest, BufferedModeCompletionFiresOnDrain) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  // Fill past the notify threshold (40 ms), so completion is deferred.
  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 6);
  bool completed = false;
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 6 / kNumChannels,
                                [&] { completed = true; }));
  EXPECT_FALSE(completed);

  // A second completion-carrying push while one is pending is rejected.
  std::vector<int16_t> extra = RampFrame(kSamplesPer10Ms);
  EXPECT_FALSE(source->PushFrame(extra.data(), kSampleRate, kNumChannels,
                                 kSamplesPer10Ms / kNumChannels,
                                 [] {}));

  // Draining to <= 40 ms buffered fires the completion.
  AdvanceMs(60);
  EXPECT_TRUE(completed);
}

TEST_F(ExternalAudioSourceTest, BufferedModeCompletionFiresInlineBelowThreshold) {
  auto source = CreateSource(kQueueSizeMs);

  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms);
  bool completed = false;
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms / kNumChannels,
                                [&] { completed = true; }));
  EXPECT_TRUE(completed);
}

TEST_F(ExternalAudioSourceTest, CompletionHandlerMayPushNextFrame) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  // Fill past the notify threshold so the completion is deferred, then push
  // the next frame from inside the handler. The handler runs outside the
  // source's lock, so this must not deadlock.
  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 6);
  bool pushed_from_handler = false;
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 6 / kNumChannels, [&] {
                                  std::vector<int16_t> next =
                                      RampFrame(kSamplesPer10Ms);
                                  pushed_from_handler = source->PushFrame(
                                      next.data(), kSampleRate, kNumChannels,
                                      kSamplesPer10Ms / kNumChannels);
                                }));

  AdvanceMs(60);
  EXPECT_TRUE(pushed_from_handler);
}

TEST_F(ExternalAudioSourceTest, ClearBufferDropsAudioAndFiresCompletion) {
  auto source = CreateSource(kQueueSizeMs);

  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 6);
  bool completed = false;
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 6 / kNumChannels,
                                [&] { completed = true; }));
  EXPECT_FALSE(completed);

  source->ClearBuffer();
  EXPECT_EQ(source->BufferedDurationMs(), 0);
  EXPECT_TRUE(completed);
}

TEST_F(ExternalAudioSourceTest, DestructionFlushesPendingCompletion) {
  auto source = CreateSource(kQueueSizeMs);

  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 6);
  bool completed = false;
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 6 / kNumChannels,
                                [&] { completed = true; }));
  EXPECT_FALSE(completed);

  source = nullptr;
  EXPECT_TRUE(completed);
}

// Runs with real threads and the real clock: mid-delivery concurrency
// cannot be exercised under the simulated time controller.
TEST(ExternalAudioSourceRealTimeTest, RemoveSinkFencesInFlightDelivery) {
  class BlockingSink : public AudioTrackSinkInterface {
   public:
    BlockingSink()
        : entered_(/*manual_reset=*/true, /*initially_signaled=*/false),
          release_(/*manual_reset=*/true, /*initially_signaled=*/false) {}

    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override {
      deliveries_.fetch_add(1);
      entered_.Set();
      release_.Wait(Event::kForever);
    }

    Event entered_;
    Event release_;
    std::atomic<int> deliveries_{0};
  };

  auto task_queue_factory = CreateDefaultTaskQueueFactory();
  auto source = ExternalAudioSource::Create(
      kSampleRate, kNumChannels, kQueueSizeMs, task_queue_factory.get());
  BlockingSink sink;
  source->AddSink(&sink);

  // Wait for the pacer to enter OnData, where it blocks holding the
  // delivery lock.
  ASSERT_TRUE(sink.entered_.Wait(TimeDelta::Seconds(5)));

  Event removed(/*manual_reset=*/true, /*initially_signaled=*/false);
  PlatformThread remover = PlatformThread::SpawnJoinable(
      [&] {
        source->RemoveSink(&sink);
        removed.Set();
      },
      "RemoveSinkFence");

  // RemoveSink must not return while a delivery to the sink is in flight.
  EXPECT_FALSE(removed.Wait(TimeDelta::Millis(200)));

  sink.release_.Set();
  EXPECT_TRUE(removed.Wait(TimeDelta::Seconds(5)));

  // Once RemoveSink has returned, no further deliveries may reach the sink
  // even though the pacer keeps ticking.
  const int deliveries_at_removal = sink.deliveries_.load();
  Event never(/*manual_reset=*/true, /*initially_signaled=*/false);
  never.Wait(TimeDelta::Millis(100));
  EXPECT_EQ(sink.deliveries_.load(), deliveries_at_removal);

  remover.Finalize();
}

TEST_F(ExternalAudioSourceTest, DestructionWhilePacerRunningIsSafe) {
  auto source = CreateSource(kQueueSizeMs);
  FakeSink sink;
  source->AddSink(&sink);

  std::vector<int16_t> data = RampFrame(kSamplesPer10Ms * 4);
  EXPECT_TRUE(source->PushFrame(data.data(), kSampleRate, kNumChannels,
                                kSamplesPer10Ms * 4 / kNumChannels));
  AdvanceMs(10);
  source = nullptr;
  // No crash, no further deliveries after destruction.
  const int frames_at_destruction = sink.frames_received_;
  AdvanceMs(30);
  EXPECT_EQ(sink.frames_received_, frames_at_destruction);
}

}  // namespace
}  // namespace webrtc
