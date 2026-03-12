/*
 * Copyright 2024 LiveKit
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

#import <Accelerate/Accelerate.h>
#import "RTCAudioRendererAdapter+Private.h"

#include <memory>

namespace webrtc {

class AudioRendererAdapter : public webrtc::AudioTrackSinkInterface {
 public:
  AudioRendererAdapter(RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter) { adapter_ = adapter; }

 private:
  __weak RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter_;

  void OnData(const void *audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames,
              std::optional<int64_t> absolute_capture_timestamp_ms) override {
    if (sample_rate <= 0 || number_of_channels == 0 || number_of_channels > 2) {
      NSLog(@"Invalid sample rate or channel count");
      return;
    }

    AVAudioFormat *format =
        [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                         sampleRate:sample_rate
                                           channels:(AVAudioChannelCount)number_of_channels
                                        interleaved:YES];

    AVAudioFrameCount frameCount = static_cast<AVAudioFrameCount>(number_of_frames);
    AVAudioPCMBuffer *pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                                frameCapacity:frameCount];
    if (!pcmBuffer) {
      NSLog(@"Failed to create AVAudioPCMBuffer");
      return;
    }

    pcmBuffer.frameLength = frameCount;

    const int16_t *inputData = static_cast<const int16_t *>(audio_data);
    const size_t copy_size = number_of_frames * number_of_channels * sizeof(int16_t);
    memcpy(pcmBuffer.int16ChannelData[0], inputData, copy_size);

    [adapter_.audioRenderer renderPCMBuffer:pcmBuffer];
  }
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCAudioRendererAdapter) {
  std::unique_ptr<webrtc::AudioRendererAdapter> _adapter;
}

@synthesize audioRenderer = _audioRenderer;

- (instancetype)initWithNativeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)audioRenderer {
  NSParameterAssert(audioRenderer);
  self = [super init];
  if (self) {
    _audioRenderer = audioRenderer;
    _adapter.reset(new webrtc::AudioRendererAdapter(self));
  }
  return self;
}

- (webrtc::AudioTrackSinkInterface *)nativeAudioRenderer {
  return _adapter.get();
}

@end
