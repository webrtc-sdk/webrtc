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
#import <os/lock.h>

#import "RTCAudioTrack+Private.h"

#import "RTCAudioRenderer.h"
#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "api/RTCAudioRendererAdapter+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace {

webrtc::AudioProcessingMode NativeAudioProcessingMode(
    RTC_OBJC_TYPE(RTCAudioProcessingMode) mode) {
  switch (mode) {
    case RTC_OBJC_TYPE(RTCAudioProcessingModePlatform):
      return webrtc::AudioProcessingMode::kPlatform;
    case RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware):
      return webrtc::AudioProcessingMode::kSoftware;
    case RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic):
    default:
      return webrtc::AudioProcessingMode::kAutomatic;
  }
}

}  // namespace

@implementation RTC_OBJC_TYPE (RTCAudioProcessingOptions)

@synthesize echoCancellation = _echoCancellation;
@synthesize noiseSuppression = _noiseSuppression;
@synthesize autoGainControl = _autoGainControl;
@synthesize highPassFilter = _highPassFilter;
@synthesize echoCancellationMode = _echoCancellationMode;
@synthesize noiseSuppressionMode = _noiseSuppressionMode;
@synthesize autoGainControlMode = _autoGainControlMode;
@synthesize highPassFilterMode = _highPassFilterMode;

- (instancetype)initWithEchoCancellation:(BOOL)echoCancellation
                        noiseSuppression:(BOOL)noiseSuppression
                         autoGainControl:(BOOL)autoGainControl
                          highPassFilter:(BOOL)highPassFilter {
  return [self
      initWithEchoCancellation:echoCancellation
              noiseSuppression:noiseSuppression
               autoGainControl:autoGainControl
                highPassFilter:highPassFilter
          echoCancellationMode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)
          noiseSuppressionMode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)
           autoGainControlMode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)
            highPassFilterMode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)];
}

- (instancetype)
    initWithEchoCancellation:(BOOL)echoCancellation
            noiseSuppression:(BOOL)noiseSuppression
             autoGainControl:(BOOL)autoGainControl
              highPassFilter:(BOOL)highPassFilter
        echoCancellationMode:
            (RTC_OBJC_TYPE(RTCAudioProcessingMode))echoCancellationMode
        noiseSuppressionMode:
            (RTC_OBJC_TYPE(RTCAudioProcessingMode))noiseSuppressionMode
         autoGainControlMode:
             (RTC_OBJC_TYPE(RTCAudioProcessingMode))autoGainControlMode
          highPassFilterMode:
              (RTC_OBJC_TYPE(RTCAudioProcessingMode))highPassFilterMode {
  self = [super init];
  if (self) {
    _echoCancellation = echoCancellation;
    _noiseSuppression = noiseSuppression;
    _autoGainControl = autoGainControl;
    _highPassFilter = highPassFilter;
    _echoCancellationMode = echoCancellationMode;
    _noiseSuppressionMode = noiseSuppressionMode;
    _autoGainControlMode = autoGainControlMode;
    _highPassFilterMode = highPassFilterMode;
  }
  return self;
}

+ (instancetype)communicationOptions {
  return [[self alloc] initWithEchoCancellation:YES
                              noiseSuppression:YES
                               autoGainControl:YES
                                highPassFilter:YES];
}

+ (instancetype)rawOptions {
  return [[self alloc] initWithEchoCancellation:NO
                              noiseSuppression:NO
                               autoGainControl:NO
                                highPassFilter:NO];
}

@end

@implementation RTC_OBJC_TYPE (RTCAudioTrack) {
  webrtc::Thread *_signalingThread;
  NSMutableArray *_adapters;
}

@synthesize source = _source;

- (instancetype)initWithFactory:
                    (RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                         source:(RTC_OBJC_TYPE(RTCAudioSource) *)source
                        trackId:(NSString *)trackId {
  RTC_DCHECK(factory);
  RTC_DCHECK(source);
  RTC_DCHECK(trackId.length);

  std::string nativeId = [NSString stdStringForString:trackId];
  webrtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      factory.nativeFactory->CreateAudioTrack(nativeId, source.nativeAudioSource.get());
  self = [self initWithFactory:factory nativeTrack:track type:RTC_OBJC_TYPE(RTCMediaStreamTrackTypeAudio)];
  if (self) {
    _source = source;
  }

  return self;
}

- (instancetype)
    initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
        nativeTrack:(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)
                        nativeTrack
               type:(RTC_OBJC_TYPE(RTCMediaStreamTrackType))type {
  NSParameterAssert(factory);
  NSParameterAssert(nativeTrack);
  NSParameterAssert(type == RTC_OBJC_TYPE(RTCMediaStreamTrackTypeAudio));
  self = [super initWithFactory:factory nativeTrack:nativeTrack type:type];
  if (self) {
    _adapters = [NSMutableArray array];
    _signalingThread = factory.signalingThread;
  }

  return self;
}

- (void)dealloc {
  // No need to switch threads when removing all renderers here;
  // just remove each sink from the native audio track directly.
  for (RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter in _adapters) {
    self.nativeAudioTrack->RemoveSink(adapter.nativeAudioRenderer);
  }
}

- (RTC_OBJC_TYPE(RTCAudioSource) *)source {
  if (!_source) {
    webrtc::scoped_refptr<webrtc::AudioSourceInterface> source(
        self.nativeAudioTrack->GetSource());
    if (source) {
      _source =
          [[RTC_OBJC_TYPE(RTCAudioSource) alloc] initWithFactory:self.factory
                                               nativeAudioSource:source];
    }
  }
  return _source;
}

- (void)addRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  if (!_signalingThread->IsCurrent()) {
    _signalingThread->BlockingCall([renderer, self] { [self addRenderer:renderer]; });
    return;
  }

  // Make sure we don't have this renderer yet.
  for (RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter in _adapters) {
    if (adapter.audioRenderer == renderer) {
      RTC_LOG(LS_INFO) << "|renderer| is already attached to this track";
      return;
    }
  }
  // Create a wrapper that provides a native pointer for us.
  RTC_OBJC_TYPE(RTCAudioRendererAdapter) *adapter =
      [[RTC_OBJC_TYPE(RTCAudioRendererAdapter) alloc] initWithNativeRenderer:renderer];
  [_adapters addObject:adapter];
  self.nativeAudioTrack->AddSink(adapter.nativeAudioRenderer);
}

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  if (!_signalingThread->IsCurrent()) {
    _signalingThread->BlockingCall([renderer, self] { [self removeRenderer:renderer]; });
    return;
  }
  __block NSUInteger indexToRemove = NSNotFound;
  [_adapters enumerateObjectsUsingBlock:^(RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter,
                                          NSUInteger idx, BOOL * stop) {
    if (adapter.audioRenderer == renderer) {
      indexToRemove = idx;
      *stop = YES;
    }
  }];
  if (indexToRemove == NSNotFound) {
    RTC_LOG(LS_INFO) << "removeRenderer called with a renderer that has not been previously added";
    return;
  }
  RTC_OBJC_TYPE(RTCAudioRendererAdapter) *adapterToRemove = [_adapters objectAtIndex:indexToRemove];
  self.nativeAudioTrack->RemoveSink(adapterToRemove.nativeAudioRenderer);
  [_adapters removeObjectAtIndex:indexToRemove];
}

- (void)removeAllRenderers {
  // Ensure the method is executed on the signaling thread.
  if (!_signalingThread->IsCurrent()) {
    _signalingThread->BlockingCall([self] { [self removeAllRenderers]; });
    return;
  }

  // Iterate over all adapters and remove each one from the native audio track.
  for (RTC_OBJC_TYPE(RTCAudioRendererAdapter) * adapter in _adapters) {
    self.nativeAudioTrack->RemoveSink(adapter.nativeAudioRenderer);
  }

  // Clear the adapters array after all sinks have been removed.
  [_adapters removeAllObjects];
}

- (BOOL)setAudioProcessingOptionsWithEchoCancellation:(BOOL)echoCancellation
                                    noiseSuppression:(BOOL)noiseSuppression
                                     autoGainControl:(BOOL)autoGainControl
                                      highPassFilter:(BOOL)highPassFilter {
  RTC_OBJC_TYPE(RTCAudioProcessingOptions) *options =
      [[RTC_OBJC_TYPE(RTCAudioProcessingOptions) alloc] initWithEchoCancellation:echoCancellation
                                                                noiseSuppression:noiseSuppression
                                                                 autoGainControl:autoGainControl
                                                                  highPassFilter:highPassFilter];
  return [self setAudioProcessingOptions:options];
}

- (BOOL)setAudioProcessingOptions:
    (RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options {
  NSParameterAssert(options);
  if (!options) {
    return NO;
  }
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->BlockingCall(
        [self, options] { return [self setAudioProcessingOptions:options]; });
  }

  webrtc::AudioOptions nativeOptions;
  nativeOptions.echo_cancellation = options.echoCancellation;
  nativeOptions.noise_suppression = options.noiseSuppression;
  nativeOptions.auto_gain_control = options.autoGainControl;
  nativeOptions.highpass_filter = options.highPassFilter;
  nativeOptions.echo_cancellation_mode =
      NativeAudioProcessingMode(options.echoCancellationMode);
  nativeOptions.noise_suppression_mode =
      NativeAudioProcessingMode(options.noiseSuppressionMode);
  nativeOptions.auto_gain_control_mode =
      NativeAudioProcessingMode(options.autoGainControlMode);
  nativeOptions.highpass_filter_mode =
      NativeAudioProcessingMode(options.highPassFilterMode);
  return self.nativeAudioTrack->SetAudioProcessingOptions(nativeOptions);
}

#pragma mark - Private

- (webrtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return webrtc::scoped_refptr<webrtc::AudioTrackInterface>(
      static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get()));
}

@end
