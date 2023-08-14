/*
 * Copyright 2023 LiveKit
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

#import "RTCAudioBuffer+Private.h"
#import "RTCAudioCustomProcessingAdapter+Private.h"

namespace webrtc {

class AudioCustomProcessingAdapter : public webrtc::CustomProcessing {
 public:
  AudioCustomProcessingAdapter(RTCAudioCustomProcessingAdapter *adapter) { adapter_ = adapter; }
  ~AudioCustomProcessingAdapter() { [adapter_.audioCustomProcessingDelegate destroy]; }

  void Initialize(int sample_rate_hz, int num_channels) override {
    if (adapter_.audioCustomProcessingDelegate != nil) {
      [adapter_.audioCustomProcessingDelegate initializeWithSampleRateHz:sample_rate_hz
                                                             numChannels:num_channels];
    }
  }

  void Process(AudioBuffer *audio_buffer) override {
    if (adapter_.audioCustomProcessingDelegate != nil) {
      RTCAudioBuffer *audioBuffer = [[RTCAudioBuffer alloc] initWithNativeType:audio_buffer];
      [adapter_.audioCustomProcessingDelegate processAudioBuffer:audioBuffer];
    }
  }

  std::string ToString() const override { return "AudioCustomProcessingAdapter"; }

 private:
  __weak RTCAudioCustomProcessingAdapter *adapter_;
};
}  // namespace webrtc

@implementation RTCAudioCustomProcessingAdapter {
  std::unique_ptr<webrtc::AudioCustomProcessingAdapter> _adapter;
}

@synthesize audioCustomProcessingDelegate = _audioCustomProcessingDelegate;

- (instancetype)initWithDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessingDelegate {
  NSParameterAssert(audioCustomProcessingDelegate);
  if (self = [super init]) {
    _audioCustomProcessingDelegate = audioCustomProcessingDelegate;
    _adapter.reset(new webrtc::AudioCustomProcessingAdapter(self));
  }

  return self;
}

- (std::unique_ptr<webrtc::CustomProcessing>)nativeAudioCustomProcessingModule {
  return std::unique_ptr<webrtc::CustomProcessing>(_adapter.get());
}

@end
