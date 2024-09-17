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
  int64_t total_frames_ = 0;

  void OnData(const void *audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    /*
     * Convert to CMSampleBuffer
     */

    if (!(number_of_channels == 1 || number_of_channels == 2)) {
      NSLog(@"RTCAudioTrack: Only mono or stereo is supported currently. numberOfChannels: %zu",
            number_of_channels);
      return;
    }

    OSStatus status;

    AudioChannelLayout acl;
    bzero(&acl, sizeof(acl));
    acl.mChannelLayoutTag =
        number_of_channels == 2 ? kAudioChannelLayoutTag_Stereo : kAudioChannelLayoutTag_Mono;

    AudioStreamBasicDescription sd;
    sd.mSampleRate = sample_rate;
    sd.mFormatID = kAudioFormatLinearPCM;
    sd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    sd.mFramesPerPacket = 1;
    sd.mChannelsPerFrame = number_of_channels;
    sd.mBitsPerChannel = bits_per_sample; /* 16 */
    sd.mBytesPerFrame = sd.mChannelsPerFrame * (sd.mBitsPerChannel / 8);
    sd.mBytesPerPacket = sd.mBytesPerFrame;

    CMSampleTimingInfo timing = {
        CMTimeMake(1, sample_rate),
        CMTimeMake(total_frames_, sample_rate),
        kCMTimeInvalid,
    };

    total_frames_ += number_of_frames;  // update the total

    CMFormatDescriptionRef format = NULL;
    status = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &sd, sizeof(acl), &acl, 0, NULL,
                                            NULL, &format);

    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to create audio format description");
      return;
    }

    CMSampleBufferRef buffer;
    status = CMSampleBufferCreate(kCFAllocatorDefault, NULL, false, NULL, NULL, format,
                                  (CMItemCount)number_of_frames, 1, &timing, 0, NULL, &buffer);
    // format is no longer required
    CFRelease(format);

    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to allocate sample buffer");
      return;
    }

    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = sd.mChannelsPerFrame;
    bufferList.mBuffers[0].mDataByteSize = (UInt32)(number_of_frames * sd.mBytesPerFrame);
    bufferList.mBuffers[0].mData = (void *)audio_data;
    status = CMSampleBufferSetDataBufferFromAudioBufferList(buffer, kCFAllocatorDefault,
                                                            kCFAllocatorDefault, 0, &bufferList);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to convert audio buffer list into sample buffer");
      return;
    }

    // Report back to RTCAudioTrack
    [adapter_.audioRenderer renderSampleBuffer:buffer];
    CFRelease(buffer);
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
