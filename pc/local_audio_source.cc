/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/local_audio_source.h"

#include "api/audio_options.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"

using webrtc::MediaSourceInterface;

namespace webrtc {

scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const AudioOptions* audio_options) {
  auto source = make_ref_counted<LocalAudioSource>(kMicrophone, nullptr);
  source->Initialize(audio_options);
  return source;
}

scoped_refptr<LocalAudioSource> LocalAudioSource::CreateWithSourceType(
    const AudioOptions* audio_options, SourceType type, AudioTransport *audio_transport) {
  auto source = make_ref_counted<LocalAudioSource>(type, audio_transport);
  source->Initialize(audio_options);
  return source;
}

void LocalAudioSource::Initialize(const AudioOptions* audio_options) {
  if (!audio_options)
    return;

  options_ = *audio_options;
}

void LocalAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  webrtc::MutexLock lock(&sink_lock_);
  if (std::find(sinks_.begin(), sinks_.end(), sink) != sinks_.end()) {
    return;  // Already added.
  }
  sinks_.push_back(sink);
}

void LocalAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  webrtc::MutexLock lock(&sink_lock_);
  auto it = std::remove(sinks_.begin(), sinks_.end(), sink);
  if (it != sinks_.end()) {
    sinks_.erase(it, sinks_.end());
  }
}

void LocalAudioSource::OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames) {
  webrtc::MutexLock lock(&sink_lock_);
  for (auto* sink : sinks_) {
    sink->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                 number_of_frames);
  }
}

void LocalAudioSource::CaptureFrame(const void* audio_data, int bits_per_sample,
                    int sample_rate, size_t number_of_channels,
                    size_t number_of_frames){
  RTC_DCHECK(audio_data);
  OnData(audio_data, bits_per_sample, sample_rate,
                              number_of_channels, number_of_frames);
}

void LocalAudioSource::SendAudioData(std::unique_ptr<AudioFrame> audio_frame) {
  OnData((const void*)audio_frame->data(), 16, audio_frame->sample_rate_hz(),
           audio_frame->num_channels(), audio_frame->samples_per_channel());
}

}  // namespace webrtc
