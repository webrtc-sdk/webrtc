/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/audio_sink.h"

#include "sdk/android/generated_peerconnection_jni/AudioTrackSink_jni.h"

namespace webrtc {
namespace jni {

AudioTrackSinkWrapper::AudioTrackSinkWrapper(JNIEnv* jni, const JavaRef<jobject>& j_sink)
    : j_sink_(jni, j_sink) {}

AudioTrackSinkWrapper::~AudioTrackSinkWrapper() {}

void AudioTrackSinkWrapper::OnData(
    const void* audio_data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  int length = (bits_per_sample / 8) * number_of_channels * number_of_frames;
  ScopedJavaLocalRef<jobject> audio_buffer =
      NewDirectByteBuffer(jni, (void *) audio_data, length);
  Java_AudioTrackSink_onData(jni, j_sink_,
      audio_buffer, bits_per_sample, sample_rate, (int) number_of_channels, (int) number_of_frames, (absolute_capture_timestamp_ms ? absolute_capture_timestamp_ms.value() : 0));
}

}  // namespace jni
}  // namespace webrtc
