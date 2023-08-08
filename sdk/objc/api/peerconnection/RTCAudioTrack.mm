/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>

#import "RTCAudioTrack+Private.h"

#import "RTCAudioRenderer.h"
#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace webrtc {
/**
 * Captures audio data and converts to CMSampleBuffers
 */
class AudioSinkConverter : public rtc::RefCountInterface, public webrtc::AudioTrackSinkInterface {
 private:
  __weak RTCAudioTrack *audioTrack_;

 public:
  AudioSinkConverter(RTCAudioTrack *audioTrack) {
    // Keep weak reference to RTCAudioTrack...
    audioTrack_ = audioTrack;
  }

  void OnData(const void *audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    /*
     * Convert to CMSampleBuffer
     * TODO: Handle case which number_of_channels could be 2 or more.
     */
    int64_t elapsed_time_ms =
        absolute_capture_timestamp_ms ? absolute_capture_timestamp_ms.value() : rtc::TimeMillis();

    OSStatus status;

    AudioChannelLayout acl;
    bzero(&acl, sizeof(acl));
    acl.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;

    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate = sample_rate;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = 1;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mBytesPerPacket = audioFormat.mFramesPerPacket * audioFormat.mChannelsPerFrame *
        audioFormat.mBitsPerChannel / 8;
    audioFormat.mBytesPerFrame = audioFormat.mBytesPerPacket / audioFormat.mFramesPerPacket;

    CMSampleTimingInfo timing = {
        CMTimeMake(1, sample_rate),
        CMTimeMake(elapsed_time_ms, 1000),
        kCMTimeInvalid,
    };

    CMFormatDescriptionRef format = NULL;
    status = CMAudioFormatDescriptionCreate(
        kCFAllocatorDefault, &audioFormat, sizeof(acl), &acl, 0, NULL, NULL, &format);

    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to create audio format description");
      return;
    }

    CMSampleBufferRef buffer;
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  NULL,
                                  false,
                                  NULL,
                                  NULL,
                                  format,
                                  (CMItemCount)number_of_frames,
                                  1,
                                  &timing,
                                  0,
                                  NULL,
                                  &buffer);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to allocate sample buffer");
      return;
    }

    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = audioFormat.mChannelsPerFrame;
    bufferList.mBuffers[0].mDataByteSize = (UInt32)(number_of_frames * audioFormat.mBytesPerFrame);
    bufferList.mBuffers[0].mData = (void *)audio_data;
    status = CMSampleBufferSetDataBufferFromAudioBufferList(
        buffer, kCFAllocatorDefault, kCFAllocatorDefault, 0, &bufferList);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to convert audio buffer list into sample buffer");
      return;
    }

    // Report back to RTCAudioTrack
    [audioTrack_ didCaptureSampleBuffer:buffer];

    CFRelease(buffer);
  }
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCAudioTrack) {
  rtc::Thread *_workerThread;
  BOOL _audioSinkAdded;
  rtc::scoped_refptr<webrtc::AudioSinkConverter> _audioConverter;
}

@synthesize source = _source;
@synthesize renderers = _renderers;

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                         source:(RTC_OBJC_TYPE(RTCAudioSource) *)source
                        trackId:(NSString *)trackId {
  RTC_DCHECK(factory);
  RTC_DCHECK(source);
  RTC_DCHECK(trackId.length);

  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      factory.nativeFactory->CreateAudioTrack(nativeId, source.nativeAudioSource.get());
  if (self = [self initWithFactory:factory nativeTrack:track type:RTCMediaStreamTrackTypeAudio]) {
    _source = source;
  }
  return self;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                    nativeTrack:(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                           type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(factory);
  NSParameterAssert(nativeTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeAudio);
  if (self = [super initWithFactory:factory nativeTrack:nativeTrack type:type]) {
    NSLog(@"RTCAudioTrack: init...");
    _workerThread = factory.workerThread;
    _renderers = [NSMutableArray<RTCAudioRenderer> array];
    _audioSinkAdded = NO;
    _audioConverter = new rtc::RefCountedObject<webrtc::AudioSinkConverter>(self);
  }

  return self;
}

- (void)dealloc {
  // Clean up...
  _workerThread->BlockingCall([self] {
    // Remove all renderers...
    [_renderers removeAllObjects];
  });
  // Remove sink if added...
  if (_audioSinkAdded) {
    self.nativeAudioTrack->RemoveSink(_audioConverter.get());
    _audioSinkAdded = NO;
  }
}

- (RTC_OBJC_TYPE(RTCAudioSource) *)source {
  if (!_source) {
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source(self.nativeAudioTrack->GetSource());
    if (source) {
      _source = [[RTC_OBJC_TYPE(RTCAudioSource) alloc] initWithFactory:self.factory
                                                     nativeAudioSource:source];
    }
  }
  return _source;
}

- (void)addRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  _workerThread->BlockingCall([self, renderer] {
    [_renderers addObject:renderer];
    // Add audio sink if not already added
    if ([_renderers count] != 0 && !_audioSinkAdded) {
      self.nativeAudioTrack->AddSink(_audioConverter.get());
      _audioSinkAdded = YES;
    }
  });
}

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  _workerThread->BlockingCall([self, renderer] {
    [_renderers removeObject:renderer];
    // Remove audio sink if no more renderers
    if ([_renderers count] == 0 && _audioSinkAdded) {
      self.nativeAudioTrack->RemoveSink(_audioConverter.get());
      _audioSinkAdded = NO;
    }
  });
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return rtc::scoped_refptr<webrtc::AudioTrackInterface>(
      static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get()));
}

- (void)didCaptureSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  // Retain reference...
  CFRetain(sampleBuffer);
  _workerThread->PostTask([self, sampleBuffer] {
    for (id<RTCAudioRenderer> renderer in _renderers) {
      [renderer renderSampleBuffer:sampleBuffer];
    }
    // Release reference...
    CFRelease(sampleBuffer);
  });
}

@end
