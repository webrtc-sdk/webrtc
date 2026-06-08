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

#include <string>

#include "api/audio/audio_processing_options_resolver.h"
#include "api/media_stream_interface.h"
#include "sdk/android/generated_peerconnection_jni/AudioTrack_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/pc/audio_sink.h"

namespace webrtc {
namespace jni {
namespace {

AudioProcessingMode AudioProcessingModeFromJava(int mode) {
  switch (mode) {
    case 1:
      return AudioProcessingMode::kPlatform;
    case 2:
      return AudioProcessingMode::kSoftware;
    case 0:
    default:
      return AudioProcessingMode::kAutomatic;
  }
}

std::string NativeAudioProcessingResult(const AudioProcessingOptionsResult &result) {
  return std::to_string(static_cast<int>(result.code)) + "\n" + result.message;
}

}  // namespace

static void JNI_AudioTrack_SetVolume(JNIEnv*, jlong j_p, jdouble volume) {
  reinterpret_cast<AudioTrackInterface*>(j_p)->SetVolume(volume);
}

static jdouble JNI_AudioTrack_GetVolume(JNIEnv*, jlong j_p) {
  return reinterpret_cast<AudioTrackInterface*>(j_p)->GetVolume();
}

static ScopedJavaLocalRef<jstring> JNI_AudioTrack_SetAudioProcessingOptions(
    JNIEnv *jni, jlong j_p, jboolean echo_cancellation, jboolean noise_suppression, jboolean auto_gain_control,
    jboolean high_pass_filter, jboolean is_echo_cancellation_platform_available,
    jboolean is_noise_suppression_platform_available, jint echo_cancellation_mode, jint noise_suppression_mode,
    jint auto_gain_control_mode, jint high_pass_filter_mode) {
  AudioTrackInterface *track = reinterpret_cast<AudioTrackInterface *>(j_p);
  AudioOptions options;
  options.echo_cancellation = static_cast<bool>(echo_cancellation);
  options.noise_suppression = static_cast<bool>(noise_suppression);
  options.auto_gain_control = static_cast<bool>(auto_gain_control);
  options.highpass_filter = static_cast<bool>(high_pass_filter);
  options.echo_cancellation_mode = AudioProcessingModeFromJava(echo_cancellation_mode);
  options.noise_suppression_mode = AudioProcessingModeFromJava(noise_suppression_mode);
  options.auto_gain_control_mode = AudioProcessingModeFromJava(auto_gain_control_mode);
  options.highpass_filter_mode = AudioProcessingModeFromJava(high_pass_filter_mode);
  AudioProcessingOptionsValidationContext validation_context;
  validation_context.is_echo_cancellation_platform_available =
      static_cast<bool>(is_echo_cancellation_platform_available);
  validation_context.is_noise_suppression_platform_available =
      static_cast<bool>(is_noise_suppression_platform_available);
  AudioProcessingOptionsResult validation = ValidateAudioProcessingOptions(options, validation_context);
  if (!validation.ok()) {
    return NativeToJavaString(jni, NativeAudioProcessingResult(validation));
  }
  AudioProcessingOptionsResult result = track->SetAudioProcessingOptions(options);
  return NativeToJavaString(jni, NativeAudioProcessingResult(result));
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
