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

#include "sdk/android/src/jni/pc/external_audio_processor.h"

namespace webrtc {

void ExternalAudioProcessorImpl::SetExternalAudioProcessing(
    ExternalAudioProcessorInterface* processor) {
  webrtc::MutexLock lock(&mutex_);
  external_processor_ = processor;
  if (initialized_) {
    external_processor_->Initialize(sample_rate_hz_, num_channels_);
  }
}

void ExternalAudioProcessorImpl::SetBypassFlag(bool enable) {
  webrtc::MutexLock lock(&mutex_);
  bypass_flag = enable;
}

void ExternalAudioProcessorImpl::Initialize(int sample_rate_hz,
                                            int num_channels) {
  webrtc::MutexLock lock(&mutex_);
  sample_rate_hz_ = sample_rate_hz;
  num_channels_ = num_channels;
  if (external_processor_) {
    external_processor_->Initialize(sample_rate_hz, num_channels);
  }
  initialized_ = true;
}

void ExternalAudioProcessorImpl::Process(webrtc::AudioBuffer* audio) {
  webrtc::MutexLock lock(&mutex_);
  if (!external_processor_ || bypass_flag) {
    return;
  }

  size_t num_frames = audio->num_frames();
  int rate = num_frames * 1000;

  if (rate != sample_rate_hz_) {
    external_processor_->Reset(rate);
    sample_rate_hz_ = rate;
  }

  external_processor_->Process(audio->num_bands(), audio->num_frames(),
                               audio->channels()[0]);
}

std::string ExternalAudioProcessorImpl::ToString() const {
  return "ExternalAudioProcessorImpl";
}

void ExternalAudioProcessorImpl::SetRuntimeSetting(
    webrtc::AudioProcessing::RuntimeSetting setting) {}

}  // namespace webrtc
