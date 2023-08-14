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

#import "RTCAudioCustomProcessingAdapter+Private.h"
#import "RTCAudioBuffer+Private.h"

namespace webrtc {

class AudioCustomProcessingAdapter : public webrtc::CustomProcessing {
 public:
  AudioCustomProcessingAdapter(RTCAudioCustomProcessingAdapter *adapter) { adapter_ = adapter; }
  ~AudioCustomProcessingAdapter() { [adapter_.audioCustomProcessing destroy]; }

  void Initialize(int sample_rate_hz, int num_channels) override {
    [adapter_.audioCustomProcessing initializeWithSampleRateHz:sample_rate_hz
                                                   numChannels:num_channels];
  }

  void Process(AudioBuffer *audio_buffer) override {
    RTCAudioBuffer *audioBuffer = [[RTCAudioBuffer alloc] initWithNativeType: audio_buffer];
    [adapter_.audioCustomProcessing processAudioBuffer: audioBuffer];
  }

  std::string ToString() const override { return "AudioCustomProcessingAdapter"; }

 private:
  __weak RTCAudioCustomProcessingAdapter *adapter_;
};
}  // namespace webrtc

@implementation RTCAudioCustomProcessingAdapter {
  std::unique_ptr<webrtc::AudioCustomProcessingAdapter> _adapter;
}

@synthesize audioCustomProcessing = _audioCustomProcessing;

- (instancetype)initWithDelegate:
    (id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessing {
  NSParameterAssert(audioCustomProcessing);
  if (self = [super init]) {
    _audioCustomProcessing = audioCustomProcessing;
    _adapter.reset(new webrtc::AudioCustomProcessingAdapter(self));
  }

  return self;
}

- (std::unique_ptr<webrtc::CustomProcessing>)nativeAudioCustomProcessingModule {
  return std::unique_ptr<webrtc::CustomProcessing>(_adapter.get());
}

@end
