/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioTrack+Private.h"

#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace webrtc {
/**
 * Captures audio data and converts to CMSampleBuffers
 */
class AudioSinkConverter : public webrtc::AudioTrackSinkInterface {
 private:
  __weak RTCAudioTrack *audioTrack_;

 public:
  AudioSinkConverter(RTCAudioTrack *audioTrack) {
    NSLog(@"AudioHook: init Hook in RTCAudioTrack");
    audioTrack_ = audioTrack;
  }

  void OnData(const void *audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    // TODO: Convert to CMSampleBuffer...
    // audioTrack_.renderers;
  }
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCAudioTrack) {
  rtc::Thread *_workerThread;
  BOOL _IsAudioConverterActive;
  std::unique_ptr<webrtc::AudioSinkConverter> _audioConverter;
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
    _renderers = [NSMutableArray<RTCAudioRenderer> array];
    _audioConverter.reset(new webrtc::AudioSinkConverter(self));
  }

  return self;
}

- (void)dealloc {
  // TODO: Clean up...
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
  [_renderers addObject:renderer];

  if ([_renderers count] != 0 && !_IsAudioConverterActive) {
    self.nativeAudioTrack->AddSink(_audioConverter.get());
    _IsAudioConverterActive
 = YES;
  }
}

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  [_renderers removeObject:renderer];

  if ([_renderers count] == 0 && _IsAudioConverterActive) {
    self.nativeAudioTrack->RemoveSink(_audioConverter.get());
    _IsAudioConverterActive
 = NO;
  }
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return rtc::scoped_refptr<webrtc::AudioTrackInterface>(
      static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get()));
}

@end
