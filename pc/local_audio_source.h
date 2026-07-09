/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_LOCAL_AUDIO_SOURCE_H_
#define PC_LOCAL_AUDIO_SOURCE_H_

#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/scoped_refptr.h"
#include "audio/audio_transport_impl.h"
#include "call/audio_sender.h"

// LocalAudioSource implements AudioSourceInterface.
// This contains settings for switching audio processing on and off.

namespace webrtc {

class LocalAudioSource : public Notifier<AudioSourceInterface>, public AudioSender {
 public:
  // Creates an instance of LocalAudioSource.
  static scoped_refptr<LocalAudioSource> Create(
      const AudioOptions* audio_options);
  // Creates an instance of LocalAudioSource with SourceType.
  static scoped_refptr<LocalAudioSource> CreateWithSourceType(
      const AudioOptions* audio_options, SourceType type = kCustom,
      AudioTransport *audio_transport = nullptr);

  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }

  const AudioOptions options() const override { return options_; }
  void SetOptions(const AudioOptions &options) override { options_ = options; }

  void AddSink(AudioTrackSinkInterface* sink) override;

  void RemoveSink(AudioTrackSinkInterface* sink) override;

  void CaptureFrame(const void* audio_data, int bits_per_sample,
                    int sample_rate, size_t number_of_channels,
                    size_t number_of_frames) override;

  SourceType GetSourceType() const override {
    return source_type_;
  }

  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;

 protected:
  LocalAudioSource(SourceType type, AudioTransport *audio_transport)
   :source_type_(type),
   audio_transport_(audio_transport) {
    if(audio_transport_) {
      audio_transport_->AddAudioSender(this);
    }
  }
  ~LocalAudioSource() override {
    webrtc::MutexLock lock(&sink_lock_);
    if (audio_transport_) {
      audio_transport_->RemoveAudioSender(this);
    }
    sinks_.clear();
  }

  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames);

 private:
  void Initialize(const AudioOptions* audio_options);

  AudioOptions options_;
  mutable webrtc::Mutex sink_lock_;
  std::vector<AudioTrackSinkInterface*> sinks_ RTC_GUARDED_BY(sink_lock_);
  SourceType source_type_;
  AudioTransport *audio_transport_ = nullptr;
};

}  // namespace webrtc

#endif  // PC_LOCAL_AUDIO_SOURCE_H_
