/*
 * Copyright 2026 LiveKit
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

#import "RTCPushAudioSource+Private.h"

#import <AVFoundation/AVFoundation.h>

#include "sdk/objc/native/src/push_audio_source.h"
#include "rtc_base/logging.h"

#include <algorithm>
#include <cmath>
#include <cstring>

@implementation RTC_OBJC_TYPE (RTCPushAudioSource) {
  rtc::scoped_refptr<webrtc::PushAudioSource> _nativeSource;
  int _sampleRate;
  int _channels;
}

- (instancetype)initWithSampleRate:(int)sampleRate channels:(int)channels {
  if ((self = [super init])) {
    _sampleRate = sampleRate;
    _channels = channels;
    _nativeSource = webrtc::PushAudioSource::Create(sampleRate, channels);
  }
  return self;
}

- (int)sampleRate {
  return _sampleRate;
}

- (int)channels {
  return _channels;
}

- (void)pushData:(const void *)data
   bitsPerSample:(int)bitsPerSample
      sampleRate:(int)sampleRate
        channels:(size_t)channels
          frames:(size_t)frames {
  if (_nativeSource) {
    _nativeSource->PushData(data, bitsPerSample, sampleRate, channels, frames);
  }
}

- (void)pushPCMBuffer:(AVAudioPCMBuffer *)buffer {
  static int pushCount = 0;
  pushCount++;

  if (!buffer || !_nativeSource) {
    if (pushCount <= 5) {
      RTC_LOG(LS_WARNING) << "pushPCMBuffer: early return - buffer=" << (buffer ? "yes" : "no")
                          << ", nativeSource=" << (_nativeSource ? "yes" : "no");
    }
    return;
  }

  AVAudioFormat *format = buffer.format;
  AVAudioChannelCount channels = format.channelCount;
  AVAudioFrameCount frames = buffer.frameLength;
  double sampleRate = format.sampleRate;

  if (pushCount <= 5) {
    RTC_LOG(LS_INFO) << "pushPCMBuffer #" << pushCount
                     << ": format=" << (int)format.commonFormat
                     << ", channels=" << channels
                     << ", frames=" << frames
                     << ", sampleRate=" << sampleRate
                     << ", isInterleaved=" << (format.isInterleaved ? "yes" : "no");
  }

  if (frames == 0) {
    RTC_LOG(LS_WARNING) << "pushPCMBuffer: frames == 0, returning";
    return;
  }

  // Allocate int16 buffer for interleaved output
  size_t sampleCount = channels * frames;
  std::vector<int16_t> int16Buffer(sampleCount);

  if (format.commonFormat == AVAudioPCMFormatFloat32) {
    if (!buffer.floatChannelData) {
      RTC_LOG(LS_WARNING) << "pushPCMBuffer: Float32 format but floatChannelData is null";
      return;
    }

    // Convert float32 to int16
    if (format.isInterleaved) {
      // Interleaved float32 input
      const float *src = buffer.floatChannelData[0];
      for (size_t i = 0; i < sampleCount; i++) {
        float sample = src[i] * 32767.0f;
        sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
        int16Buffer[i] = static_cast<int16_t>(sample);
      }
    } else {
      // Non-interleaved (planar) float32 input - need to interleave
      for (AVAudioFrameCount frame = 0; frame < frames; frame++) {
        for (AVAudioChannelCount ch = 0; ch < channels; ch++) {
          float sample = buffer.floatChannelData[ch][frame] * 32767.0f;
          sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
          int16Buffer[frame * channels + ch] = static_cast<int16_t>(sample);
        }
      }
    }
  } else if (format.commonFormat == AVAudioPCMFormatInt16) {
    if (!buffer.int16ChannelData) {
      RTC_LOG(LS_WARNING) << "pushPCMBuffer: Int16 format but int16ChannelData is null";
      return;
    }

    if (format.isInterleaved) {
      const int16_t *src = buffer.int16ChannelData[0];
      memcpy(int16Buffer.data(), src, sampleCount * sizeof(int16_t));
    } else {
      for (AVAudioFrameCount frame = 0; frame < frames; frame++) {
        for (AVAudioChannelCount ch = 0; ch < channels; ch++) {
          int16Buffer[frame * channels + ch] = buffer.int16ChannelData[ch][frame];
        }
      }
    }
  } else {
    RTC_LOG(LS_WARNING) << "pushPCMBuffer: Unsupported format: " << (int)format.commonFormat;
    return;
  }

  if (pushCount <= 5) {
    RTC_LOG(LS_INFO) << "pushPCMBuffer: calling PushData with " << sampleCount << " samples";
  }
  _nativeSource->PushData(int16Buffer.data(), 16, static_cast<int>(sampleRate),
                          channels, frames);
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioSourceInterface>)nativeAudioSource {
  return _nativeSource;
}

@end
