/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCExternalAudioSource+Private.h"

#import "RTCAudioSource+Private.h"
#import "helpers/NSString+StdString.h"

#include <vector>

#include "common_audio/include/audio_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace {

// Converts an AVAudioPCMBuffer (int16/float32, interleaved or not) to
// interleaved int16. Returns false for unsupported formats.
bool ConvertPCMBufferToInterleavedS16(AVAudioPCMBuffer *buffer,
                                      std::vector<int16_t> &out) {
  const AVAudioFrameCount frames = buffer.frameLength;
  const AVAudioChannelCount channels = buffer.format.channelCount;
  const bool interleaved = buffer.format.isInterleaved;
  out.resize(frames * channels);

  switch (buffer.format.commonFormat) {
    case AVAudioPCMFormatInt16: {
      if (interleaved) {
        std::memcpy(out.data(), buffer.int16ChannelData[0],
                    out.size() * sizeof(int16_t));
      } else {
        for (AVAudioChannelCount ch = 0; ch < channels; ++ch) {
          const int16_t *src = buffer.int16ChannelData[ch];
          for (AVAudioFrameCount i = 0; i < frames; ++i) {
            out[i * channels + ch] = src[i];
          }
        }
      }
      return true;
    }
    case AVAudioPCMFormatFloat32: {
      if (interleaved) {
        webrtc::FloatToS16(buffer.floatChannelData[0], out.size(), out.data());
      } else {
        for (AVAudioChannelCount ch = 0; ch < channels; ++ch) {
          const float *src = buffer.floatChannelData[ch];
          for (AVAudioFrameCount i = 0; i < frames; ++i) {
            out[i * channels + ch] = webrtc::FloatToS16(src[i]);
          }
        }
      }
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

@implementation RTC_OBJC_TYPE (RTCExternalAudioSource) {
  webrtc::scoped_refptr<webrtc::ExternalAudioSource> _nativeExternalAudioSource;
}

@synthesize nativeExternalAudioSource = _nativeExternalAudioSource;

- (instancetype)
        initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
    nativeExternalAudioSource:
        (webrtc::scoped_refptr<webrtc::ExternalAudioSource>)nativeSource {
  RTC_DCHECK(factory);
  RTC_DCHECK(nativeSource);

  self = [super initWithFactory:factory nativeAudioSource:nativeSource];
  if (self) {
    _nativeExternalAudioSource = nativeSource;
  }
  return self;
}

- (instancetype)
      initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
    nativeAudioSource:
        (webrtc::scoped_refptr<webrtc::AudioSourceInterface>)nativeAudioSource {
  RTC_DCHECK_NOTREACHED();
  return nil;
}

- (int)sampleRate {
  return _nativeExternalAudioSource->sample_rate_hz();
}

- (NSUInteger)channels {
  return _nativeExternalAudioSource->num_channels();
}

- (int)queueSizeMs {
  return _nativeExternalAudioSource->queue_size_ms();
}

- (int64_t)bufferedDurationMs {
  return _nativeExternalAudioSource->BufferedDurationMs();
}

- (BOOL)captureFrameWithData:(NSData *)pcmData
                  sampleRate:(int)sampleRate
                    channels:(NSUInteger)channels
                      frames:(NSUInteger)frames
           completionHandler:(nullable void (^)(void))completionHandler {
  if (pcmData.length != frames * channels * sizeof(int16_t)) {
    RTC_LOG(LS_WARNING)
        << "RTCExternalAudioSource: data length " << pcmData.length
        << " does not match " << frames << " frames x " << channels
        << " channels of int16";
    return NO;
  }
  return [self captureBytes:static_cast<const int16_t *>(pcmData.bytes)
                 sampleRate:sampleRate
                   channels:channels
                     frames:frames
          completionHandler:completionHandler];
}

- (BOOL)captureAVAudioPCMBuffer:(AVAudioPCMBuffer *)pcmBuffer
              completionHandler:(nullable void (^)(void))completionHandler {
  std::vector<int16_t> interleaved;
  if (!ConvertPCMBufferToInterleavedS16(pcmBuffer, interleaved)) {
    RTC_LOG(LS_WARNING) << "RTCExternalAudioSource: unsupported "
                           "AVAudioPCMBuffer format "
                        << pcmBuffer.format.commonFormat;
    return NO;
  }
  return [self captureBytes:interleaved.data()
                 sampleRate:(int)pcmBuffer.format.sampleRate
                   channels:pcmBuffer.format.channelCount
                     frames:pcmBuffer.frameLength
          completionHandler:completionHandler];
}

- (BOOL)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer
          completionHandler:(nullable void (^)(void))completionHandler {
  if (!sampleBuffer || !CMSampleBufferIsValid(sampleBuffer) ||
      !CMSampleBufferDataIsReady(sampleBuffer)) {
    return NO;
  }
  CMFormatDescriptionRef description =
      CMSampleBufferGetFormatDescription(sampleBuffer);
  const AudioStreamBasicDescription *asbd =
      CMAudioFormatDescriptionGetStreamBasicDescription(description);
  if (!asbd || asbd->mFormatID != kAudioFormatLinearPCM) {
    return NO;
  }

  AVAudioFormat *format =
      [[AVAudioFormat alloc] initWithStreamDescription:asbd];
  if (!format) {
    return NO;
  }
  const CMItemCount numFrames = CMSampleBufferGetNumSamples(sampleBuffer);
  AVAudioPCMBuffer *pcmBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                    frameCapacity:(AVAudioFrameCount)numFrames];
  if (!pcmBuffer) {
    return NO;
  }
  pcmBuffer.frameLength = (AVAudioFrameCount)numFrames;
  OSStatus status = CMSampleBufferCopyPCMDataIntoAudioBufferList(
      sampleBuffer, 0, (int32_t)numFrames, pcmBuffer.mutableAudioBufferList);
  if (status != noErr) {
    RTC_LOG(LS_WARNING) << "RTCExternalAudioSource: failed to copy PCM data "
                           "from CMSampleBuffer, status "
                        << status;
    return NO;
  }
  return [self captureAVAudioPCMBuffer:pcmBuffer
                     completionHandler:completionHandler];
}

- (void)clearBuffer {
  _nativeExternalAudioSource->ClearBuffer();
}

#pragma mark - Private

- (BOOL)captureBytes:(const int16_t *)data
          sampleRate:(int)sampleRate
            channels:(NSUInteger)channels
              frames:(NSUInteger)frames
   completionHandler:(nullable void (^)(void))completionHandler {
  absl::AnyInvocable<void() &&> onComplete = nullptr;
  if (completionHandler) {
    onComplete = [handler = completionHandler]() mutable {
      handler();
    };
  }
  return _nativeExternalAudioSource->PushFrame(
      data, sampleRate, channels, frames, std::move(onComplete));
}

@end
