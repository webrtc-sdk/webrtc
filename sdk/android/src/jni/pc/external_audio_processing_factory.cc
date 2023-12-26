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

#include "sdk/android/src/jni/pc/external_audio_processing_factory.h"

#include <jni.h>
#include <syslog.h>

#include "api/make_ref_counted.h"
#include "rtc_base/ref_counted_object.h"
#include "sdk/android/generated_peerconnection_jni/ExternalAudioProcessingFactory_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/external_audio_processor.h"

namespace webrtc {
namespace jni {

ExternalAudioProcessingJni::ExternalAudioProcessingJni(
    JNIEnv* jni,
    const JavaRef<jobject>& j_processing)
    : j_processing_global_(jni, j_processing) {}
ExternalAudioProcessingJni::~ExternalAudioProcessingJni() {}
void ExternalAudioProcessingJni::Initialize(int sample_rate_hz,
                                            int num_channels) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioProcessing_initialize(env, j_processing_global_, sample_rate_hz,
                                  num_channels);
}

void ExternalAudioProcessingJni::Reset(int new_rate) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioProcessing_reset(env, j_processing_global_, new_rate);
}

void ExternalAudioProcessingJni::Process(int num_bands, int num_frames, int buffer_size, float* buffer) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> audio_buffer =
      NewDirectByteBuffer(env, (void*)buffer, buffer_size * sizeof(float));
  Java_AudioProcessing_process(env, j_processing_global_, num_bands, num_frames, audio_buffer);
}

ExternalAudioProcessingFactory::ExternalAudioProcessingFactory() {
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

static ExternalAudioProcessingFactory* default_processor_ptr;

static jlong JNI_ExternalAudioProcessingFactory_GetDefaultApm(JNIEnv* env) {
  if (!default_processor_ptr) {
    auto default_processor = rtc::make_ref_counted<ExternalAudioProcessingFactory>();
    default_processor_ptr = default_processor.release();
  }
  return webrtc::jni::jlongFromPointer(default_processor_ptr->apm().get());
}

static jlong JNI_ExternalAudioProcessingFactory_SetCapturePostProcessing(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_processing) {
  if (!default_processor_ptr) {
    return 0;
  }
  auto processing =
      rtc::make_ref_counted<ExternalAudioProcessingJni>(env, j_processing);
  processing->AddRef();
  default_processor_ptr->capture_post_processor()->SetExternalAudioProcessing(
      processing.get());
  return jlongFromPointer(processing.get());
}

static jlong JNI_ExternalAudioProcessingFactory_SetRenderPreProcessing(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_processing) {
  if (!default_processor_ptr) {
    return 0;
  }
  auto processing =
      rtc::make_ref_counted<ExternalAudioProcessingJni>(env, j_processing);
  processing->AddRef();
  default_processor_ptr->render_pre_processor()->SetExternalAudioProcessing(
      processing.get());
  return jlongFromPointer(processing.get());
}

static void JNI_ExternalAudioProcessingFactory_SetBypassFlagForCapturePost(
    JNIEnv* env,
    jboolean bypass) {
  if (!default_processor_ptr) {
    return;
  }
  default_processor_ptr->capture_post_processor()->SetBypassFlag(bypass);
}

static void JNI_ExternalAudioProcessingFactory_SetBypassFlagForRenderPre(
    JNIEnv* env,
    jboolean bypass) {
  if (!default_processor_ptr) {
    return;
  }
  default_processor_ptr->render_pre_processor()->SetBypassFlag(bypass);
}

static void JNI_ExternalAudioProcessingFactory_Destroy(JNIEnv* env) {
  if (!default_processor_ptr) {
    return;
  }
  default_processor_ptr->render_pre_processor()->SetExternalAudioProcessing(
      nullptr);
  default_processor_ptr->capture_post_processor()->SetExternalAudioProcessing(
      nullptr);
  delete default_processor_ptr;
}

}  // namespace jni
}  // namespace webrtc
