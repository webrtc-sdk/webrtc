/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/audio_track.h"

#include <string>

#include "absl/strings/string_view.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_stream_track.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/logging.h"

namespace webrtc {

// static
scoped_refptr<AudioTrack> AudioTrack::Create(
    absl::string_view id,
    const scoped_refptr<AudioSourceInterface>& source) {
  return make_ref_counted<AudioTrack>(id, source);
}

AudioTrack::AudioTrack(absl::string_view label,
                       const scoped_refptr<AudioSourceInterface>& source)
    : MediaStreamTrack<AudioTrackInterface>(label), audio_source_(source) {
  if (audio_source_) {
    audio_source_->RegisterObserver(this);
    OnChanged();
  }
}

AudioTrack::~AudioTrack() {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);
  set_state(MediaStreamTrackInterface::kEnded);
  if (audio_source_)
    audio_source_->UnregisterObserver(this);
}

std::string AudioTrack::kind() const {
  return kAudioKind;
}

AudioSourceInterface* AudioTrack::GetSource() const {
  // Callable from any thread.
  return audio_source_.get();
}

AudioProcessingOptionsResult AudioTrack::SetAudioProcessingOptions(const AudioOptions &options) {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);
  if (!audio_source_ || audio_source_->remote()) {
    return AudioProcessingOptionsResult::Rejected(AudioProcessingOptionsResultCode::kRejectedRemoteTrack,
                                                  "Audio processing options can only be set on local audio tracks");
  }
  RTC_LOG(LS_INFO) << "AudioTrack::SetAudioProcessingOptions: " << options.ToString();

  AudioOptions processing_options;
  processing_options.echo_cancellation = options.echo_cancellation;
  processing_options.echo_cancellation_mode = options.echo_cancellation_mode;
  processing_options.noise_suppression = options.noise_suppression;
  processing_options.noise_suppression_mode = options.noise_suppression_mode;
  processing_options.auto_gain_control = options.auto_gain_control;
  processing_options.auto_gain_control_mode = options.auto_gain_control_mode;
  processing_options.highpass_filter = options.highpass_filter;
  processing_options.highpass_filter_mode = options.highpass_filter_mode;

  AudioOptions updated_options = audio_source_->options();
  updated_options.SetAll(processing_options);
  audio_source_->SetOptions(updated_options);
  FireOnChanged();
  return AudioProcessingOptionsResult::Stored();
}

void AudioTrack::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);
  if (audio_source_)
    audio_source_->AddSink(sink);
}

void AudioTrack::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);
  if (audio_source_)
    audio_source_->RemoveSink(sink);
}

void AudioTrack::OnChanged() {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);
  if (audio_source_->state() == MediaSourceInterface::kEnded) {
    set_state(kEnded);
  } else {
    set_state(kLive);
  }
}

}  // namespace webrtc
