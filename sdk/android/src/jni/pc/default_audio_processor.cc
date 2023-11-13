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

#include "rtc_base/ref_counted_object.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/external_audio_processor.h"
#include "sdk/android/src/jni/pc/external_audio_processor_interface.h"

namespace webrtc {
namespace jni {

static webrtc::AudioProcessing* apm_ptr = nullptr;
static ExternalAudioProcessorImpl* capture_post_processor_ptr = nullptr;
static ExternalAudioProcessorImpl* render_pre_processor_ptr = nullptr;

static jlong JNI_ExternalAudioProcessor_GetDefaultApm(JNIEnv* env) {
  std::unique_ptr<webrtc::CustomProcessing> capture_post_processor(
      capture_post_processor_ptr);
  std::unique_ptr<webrtc::CustomProcessing> render_pre_processor(
      render_pre_processor_ptr);
  auto apm = webrtc::AudioProcessingBuilder()
                 .SetCapturePostProcessing(std::move(capture_post_processor))
                 .SetRenderPreProcessing(std::move(render_pre_processor))
                 .Create();
  webrtc::AudioProcessing::Config config;
  config.echo_canceller.enabled = false;
  config.echo_canceller.mobile_mode = true;
  apm->ApplyConfig(config);
  apm_ptr = apm.release();
  return webrtc::jni::jlongFromPointer(apm_ptr);
}

static void JNI_ExternalAudioProcessor_SetCapturePostProcessing(
    JNIEnv* env,
    jlong externalProcessorPointer) {
  ExternalAudioProcessorInterface* externalProcessor =
      reinterpret_cast<ExternalAudioProcessorInterface*>(
          externalProcessorPointer);
  if (externalProcessor == nullptr || capture_post_processor_ptr == nullptr) {
    return;
  }
  capture_post_processor_ptr->SetExternalAudioProcessing(externalProcessor);
}

static void JNI_ExternalAudioProcessor_SetRenderPreProcessing(
    JNIEnv* env,
    jlong externalProcessorPointer) {
  ExternalAudioProcessorInterface* externalProcessor =
      reinterpret_cast<ExternalAudioProcessorInterface*>(
          externalProcessorPointer);
  if (externalProcessor == nullptr || render_pre_processor_ptr == nullptr) {
    return;
  }
  render_pre_processor_ptr->SetExternalAudioProcessing(externalProcessor);
}

static void JNI_ExternalAudioProcessor_SetBypassFlagForCapturePostProcessing(
    JNIEnv* env,
    jboolean disable) {
  if (capture_post_processor_ptr == nullptr) {
    return;
  }
  capture_post_processor_ptr->SetBypassFlag(disable);
}

static void JNI_ExternalAudioProcessor_SetBypassFlagForRenderPreProcessing(
    JNIEnv* env,
    jboolean disable) {
  if (render_pre_processor_ptr == nullptr) {
    return;
  }
  render_pre_processor_ptr->SetBypassFlag(disable);
}

static void JNI_ExternalAudioProcessor_Destroy(JNIEnv* env) {
  delete apm_ptr;
  capture_post_processor_ptr = nullptr;
  render_pre_processor_ptr = nullptr;
}

}  // namespace jni
}  // namespace webrtc
