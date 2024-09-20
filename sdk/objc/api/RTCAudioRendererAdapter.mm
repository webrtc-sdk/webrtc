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
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    OSStatus status;
    AudioChannelLayout acl;
    bzero(&acl, sizeof(acl));
    acl.mChannelLayoutTag =
        number_of_channels == 2 ? kAudioChannelLayoutTag_Stereo : kAudioChannelLayoutTag_Mono;

    // Create AudioStreamBasicDescription for float format
    AudioStreamBasicDescription sd;
    sd.mSampleRate = sample_rate;
    sd.mFormatID = kAudioFormatLinearPCM;
    sd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    sd.mFramesPerPacket = 1;
    sd.mChannelsPerFrame = number_of_channels;
    sd.mBitsPerChannel = 32;  // 32-bit float
    sd.mBytesPerFrame = sd.mChannelsPerFrame * (sd.mBitsPerChannel / 8);
    sd.mBytesPerPacket = sd.mBytesPerFrame;

    CMFormatDescriptionRef format = NULL;
    status = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &sd, sizeof(acl), &acl, 0, NULL,
                                            NULL, &format);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to create audio format description");
      return;
    }

    AVAudioFormat *format2 = [[AVAudioFormat alloc] initWithCMAudioFormatDescription:format];
    CFRelease(format);

    AVAudioFrameCount frameCount = (AVAudioFrameCount)number_of_frames;
    AVAudioPCMBuffer *pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format2
                                                                frameCapacity:frameCount];

    if (pcmBuffer == nil) {
      NSLog(@"Failed to create AVAudioPCMBuffer");
      return;
    }

    pcmBuffer.frameLength = frameCount;

    const int16_t *inputData = static_cast<const int16_t *>(audio_data);
    float scale = 1.0f / 32768.0f;

    for (size_t channel = 0; channel < number_of_channels; ++channel) {
      vDSP_vflt16(inputData + channel * number_of_frames, 1, pcmBuffer.floatChannelData[channel], 1,
                  frameCount);
      vDSP_vsmul(pcmBuffer.floatChannelData[channel], 1, &scale,
                 pcmBuffer.floatChannelData[channel], 1, frameCount);
    }

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
