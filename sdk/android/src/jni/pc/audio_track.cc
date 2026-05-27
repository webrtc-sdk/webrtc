/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "api/media_stream_interface.h"
#include "sdk/android/src/jni/pc/audio_sink.h"

#include "sdk/android/generated_peerconnection_jni/AudioTrack_jni.h"

namespace webrtc {
namespace jni {

static void JNI_AudioTrack_SetVolume(JNIEnv*, jlong j_p, jdouble volume) {
  reinterpret_cast<AudioTrackInterface*>(j_p)->SetVolume(volume);
}

static jdouble JNI_AudioTrack_GetVolume(JNIEnv*, jlong j_p) {
  return reinterpret_cast<AudioTrackInterface*>(j_p)->GetVolume();
}

static jboolean JNI_AudioTrack_SetAudioProcessingOptions(
    JNIEnv*,
    jlong j_p,
    jboolean echo_cancellation,
    jboolean noise_suppression,
    jboolean auto_gain_control,
    jboolean high_pass_filter) {
  AudioTrackInterface* track = reinterpret_cast<AudioTrackInterface*>(j_p);
  AudioSourceInterface* source = track->GetSource();
  if (!source || source->remote()) {
    return false;
  }

  AudioOptions options = source->options();
  options.echo_cancellation = static_cast<bool>(echo_cancellation);
  options.noise_suppression = static_cast<bool>(noise_suppression);
  options.auto_gain_control = static_cast<bool>(auto_gain_control);
  options.highpass_filter = static_cast<bool>(high_pass_filter);
  source->SetOptions(options);
  return true;
}

static void JNI_AudioTrack_AddSink(JNIEnv* jni,
                                   jlong j_native_track,
                                   jlong j_native_sink) {
  reinterpret_cast<AudioTrackInterface*>(j_native_track)
      ->AddSink(reinterpret_cast<webrtc::AudioTrackSinkInterface*>(j_native_sink));
}

static void JNI_AudioTrack_RemoveSink(JNIEnv* jni,
                                      jlong j_native_track,
                                      jlong j_native_sink) {
  reinterpret_cast<AudioTrackInterface*>(j_native_track)
      ->RemoveSink(reinterpret_cast<webrtc::AudioTrackSinkInterface*>(j_native_sink));
}

static jlong JNI_AudioTrack_WrapSink(JNIEnv* jni,
                                     const JavaParamRef<jobject>& sink) {
  return jlongFromPointer(new AudioTrackSinkWrapper(jni, sink));
}

static void JNI_AudioTrack_FreeSink(JNIEnv* jni, jlong j_native_sink) {
  delete reinterpret_cast<jni::AudioTrackSinkWrapper*>(j_native_sink);
}


}  // namespace jni
}  // namespace webrtc
