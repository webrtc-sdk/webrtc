/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_stream_interface.h"
#include "sdk/android/src/jni/pc/audio_sink.h"

#include "sdk/android/generated_peerconnection_jni/AudioTrack_jni.h"

namespace webrtc {
namespace jni {

static void JNI_AudioTrack_SetVolume(JNIEnv*, jlong j_p, jdouble volume) {
  rtc::scoped_refptr<AudioSourceInterface> source(
      reinterpret_cast<AudioTrackInterface*>(j_p)->GetSource());
  source->SetVolume(volume);
}

static void JNI_AudioTrack_AddSink(JNIEnv* jni,
                                   jlong j_native_track,
                                   jlong j_native_sink) {
  reinterpret_cast<AudioTrackInterface*>(j_native_track)
      ->AddSink(reinterpret_cast<rtc::AudioSinkInterface*>(j_native_sink));
}

static void JNI_AudioTrack_RemoveSink(JNIEnv* jni,
                                      jlong j_native_track,
                                      jlong j_native_sink) {
  reinterpret_cast<AudioTrackInterface*>(j_native_track)
      ->RemoveSink(reinterpret_cast<rtc::AudioSinkInterface*>(j_native_sink));
}

static jlong JNI_AudioTrack_WrapSink(JNIEnv* jni,
                                     const JavaParamRef<jobject>& sink) {
  return jlongFromPointer(new VideoSinkWrapper(jni, sink));
}

static void JNI_AudioTrack_FreeSink(JNIEnv* jni, jlong j_native_sink) {
  delete reinterpret_cast<rtc::VideoSinkInterface<VideoFrame>*>(j_native_sink);
}


}  // namespace jni
}  // namespace webrtc
