/*
 * Copyright 2022 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sdk/android/src/jni/pc/default_audio_processor.h"

#include <jni.h>
#include <syslog.h>

#include "api/make_ref_counted.h"
#include "rtc_base/ref_counted_object.h"
#include "sdk/android/generated_peerconnection_jni/ExternalAudioProcessor_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/external_audio_processor.h"
#include "sdk/android/src/jni/pc/external_audio_processor_interface.h"

namespace webrtc {
namespace jni {

ExternalAudioProcessingJni::ExternalAudioProcessingJni(
    JNIEnv* jni,
    const JavaRef<jobject>& j_prosessing)
    : j_prosessing_global_(jni, j_prosessing) {}
ExternalAudioProcessingJni::~ExternalAudioProcessingJni() {}
void ExternalAudioProcessingJni::Initialize(int sample_rate_hz,
                                            int num_channels) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioProcessing_Initialize(env, j_prosessing_global_, sample_rate_hz,
                                  num_channels);
}

void ExternalAudioProcessingJni::Reset(int new_rate) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioProcessing_Reset(env, j_prosessing_global_, new_rate);
}

void ExternalAudioProcessingJni::Process(int num_bans,
                                         int buffer_size,
                                         float* buffer) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> audio_buffer =
      NewDirectByteBuffer(env, (void*)buffer, buffer_size * sizeof(float));
  Java_AudioProcessing_Process(env, j_prosessing_global_, num_bans,
                               audio_buffer);
}

DefaultAudioProcessor::DefaultAudioProcessor() {
  capture_post_processor_ = new ExternalAudioProcessor();
  std::unique_ptr<webrtc::CustomProcessing> capture_post_processor(
      capture_post_processor_);

  render_pre_processor_ = new ExternalAudioProcessor();
  std::unique_ptr<webrtc::CustomProcessing> render_pre_processor(
      render_pre_processor_);

  apm_ = webrtc::AudioProcessingBuilder()
             .SetCapturePostProcessing(std::move(capture_post_processor))
             .SetRenderPreProcessing(std::move(render_pre_processor))
             .Create();

  webrtc::AudioProcessing::Config config;
  config.echo_canceller.enabled = false;
  config.echo_canceller.mobile_mode = true;
  apm_->ApplyConfig(config);
}

static rtc::scoped_refptr<DefaultAudioProcessor> default_processor;

static jlong JNI_ExternalAudioProcessor_GetDefaultApm(JNIEnv* env) {
  if (!default_processor) {
    default_processor = rtc::make_ref_counted<DefaultAudioProcessor>();
  }
  return webrtc::jni::jlongFromPointer(default_processor->apm().get());
}

static jlong JNI_ExternalAudioProcessor_SetCapturePostProcessing(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_processing) {
  if (!default_processor) {
    return 0;
  }
  auto processing =
      rtc::make_ref_counted<ExternalAudioProcessingJni>(env, j_processing);
  processing->AddRef();
  default_processor->capture_post_processor()->SetExternalAudioProcessing(
      processing.get());
  return jlongFromPointer(processing.get());
}

static jlong JNI_ExternalAudioProcessor_SetRenderPreProcessing(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_processing) {
  if (!default_processor) {
    return 0;
  }
  auto processing =
      rtc::make_ref_counted<ExternalAudioProcessingJni>(env, j_processing);
  processing->AddRef();
  default_processor->render_pre_processor()->SetExternalAudioProcessing(
      processing.get());
  return jlongFromPointer(processing.get());
}

static void JNI_ExternalAudioProcessor_SetBypassFlagForCapturePost(
    JNIEnv* env,
    jboolean bypass) {
  if (!default_processor) {
    return;
  }
  default_processor->capture_post_processor()->SetBypassFlag(bypass);
}

static void JNI_ExternalAudioProcessor_SetBypassFlagForRenderPre(
    JNIEnv* env,
    jboolean bypass) {
  if (!default_processor) {
    return;
  }
  default_processor->render_pre_processor()->SetBypassFlag(bypass);
}

static void JNI_ExternalAudioProcessor_Destroy(JNIEnv* env, jlong ptr) {
  if (!default_processor) {
    return;
  }
  default_processor->render_pre_processor()->SetExternalAudioProcessing(
      nullptr);
  default_processor->capture_post_processor()->SetExternalAudioProcessing(
      nullptr);
}

}  // namespace jni
}  // namespace webrtc
