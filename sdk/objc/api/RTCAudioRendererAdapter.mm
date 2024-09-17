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
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    // Create AVAudioFormat
    AVAudioFormat *format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                             sampleRate:sample_rate
                                                               channels:number_of_channels
                                                            interleaved:YES];

    // Calculate the buffer length in frames
    AVAudioFrameCount frameCount = (AVAudioFrameCount)number_of_frames;

    // Create AVAudioPCMBuffer
    AVAudioPCMBuffer *pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                                frameCapacity:frameCount];

    // Ensure we have enough space in the buffer
    if (pcmBuffer == nil) {
      NSLog(@"Failed to create AVAudioPCMBuffer");
      return;
    }

    pcmBuffer.frameLength = frameCount;

    // Copy the audio data to the PCM buffer
    memcpy(pcmBuffer.int16ChannelData[0], audio_data,
           number_of_frames * sizeof(int16_t) * number_of_channels);

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
  if (self = [super init]) {
    _audioRenderer = audioRenderer;
    _adapter.reset(new webrtc::AudioRendererAdapter(self));
  }
  return self;
}

- (webrtc::AudioTrackSinkInterface *)nativeAudioRenderer {
  return _adapter.get();
}

@end
