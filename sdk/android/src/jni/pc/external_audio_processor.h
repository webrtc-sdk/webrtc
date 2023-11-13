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

#ifndef SDK_ANDROID_SRC_JNI_PC_EXTERNAL_AUDIO_PROCESSOR_H_
#define SDK_ANDROID_SRC_JNI_PC_EXTERNAL_AUDIO_PROCESSOR_H_


#define WEBRTC_APM_DEBUG_DUMP 0

#include "sdk/android/src/jni/pc/external_audio_processor_interface.h"

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class ExternalAudioProcessorImpl : public webrtc::CustomProcessing {
 public:
  ExternalAudioProcessorImpl() = default;
  ~ExternalAudioProcessorImpl() override = default;

  void SetExternalAudioProcessing(ExternalAudioProcessorInterface* processor);
  void SetBypassFlag(bool enable);

  static ExternalAudioProcessorImpl* SharedInstance() {
    if (shared_instance_ == nullptr) {
      shared_instance_ = new ExternalAudioProcessorImpl();
    }
    return shared_instance_;
  }

 private:
  void Initialize(int sample_rate_hz, int num_channels) override;
  void Process(webrtc::AudioBuffer* audio) override;
  std::string ToString() const override;
  void SetRuntimeSetting(
      webrtc::AudioProcessing::RuntimeSetting setting) override;

 private:
  mutable webrtc::Mutex mutex_;
  ExternalAudioProcessorInterface* external_processor_;
  bool bypass_flag = false;
  bool initialized_ = false;
  int sample_rate_hz_;
  int num_channels_;
  static ExternalAudioProcessorImpl* shared_instance_;
};

}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_EXTERNAL_AUDIO_PROCESSOR_H_
