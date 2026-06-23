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
#import <TargetConditionals.h>

#import "RTCAudioTrack+Private.h"

#import "RTCAudioProcessingOptions+Private.h"
#import "RTCAudioRenderer.h"
#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "api/RTCAudioRendererAdapter+Private.h"
#import "helpers/NSString+StdString.h"

#include "api/audio/audio_processing_options_resolver.h"
#include "rtc_base/checks.h"

// clang-format off
@interface RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) ()

- (instancetype)initWithCode:(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode))code
                     message:(NSString *)message;

@end
// clang-format on

@implementation RTC_OBJC_TYPE (RTCAudioProcessingOptionsResult)

@synthesize success = _success;
@synthesize code = _code;
@synthesize message = _message;

- (instancetype)initWithCode:(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode))code message:(NSString *)message {
  self = [super init];
  if (self) {
    _code = code;
    _message = [message copy];
    _success = code == RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplied) ||
               code == RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeStored);
  }
  return self;
}

@end

@implementation RTC_OBJC_TYPE (RTCAudioProcessingComponentOptions)

@synthesize enabled = _enabled;
@synthesize mode = _mode;

- (instancetype)initWithEnabled:(BOOL)enabled {
  return [self initWithEnabled:enabled mode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)];
}

- (instancetype)initWithEnabled:(BOOL)enabled mode:(RTC_OBJC_TYPE(RTCAudioProcessingMode))mode {
  self = [super init];
  if (self) {
    _enabled = enabled;
    _mode = mode;
  }
  return self;
}

@end

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
  return [self initWithEchoCancellationOptions:[[RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) alloc]
                                                   initWithEnabled:echoCancellation
                                                              mode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)]
                       noiseSuppressionOptions:[[RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) alloc]
                                                   initWithEnabled:noiseSuppression
                                                              mode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)]
                        autoGainControlOptions:[[RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) alloc]
                                                   initWithEnabled:autoGainControl
                                                              mode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)]
                         highPassFilterOptions:[[RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) alloc]
                                                   initWithEnabled:highPassFilter
                                                              mode:RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)]];
}

- (instancetype)
    initWithEchoCancellationOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)echoCancellationOptions
            noiseSuppressionOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)noiseSuppressionOptions
             autoGainControlOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)autoGainControlOptions
              highPassFilterOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)highPassFilterOptions {
  NSParameterAssert(echoCancellationOptions);
  NSParameterAssert(noiseSuppressionOptions);
  NSParameterAssert(autoGainControlOptions);
  NSParameterAssert(highPassFilterOptions);
  self = [super init];
  if (self) {
    _echoCancellation = echoCancellationOptions.enabled;
    _noiseSuppression = noiseSuppressionOptions.enabled;
    _autoGainControl = autoGainControlOptions.enabled;
    _highPassFilter = highPassFilterOptions.enabled;
    _echoCancellationMode = echoCancellationOptions.mode;
    _noiseSuppressionMode = noiseSuppressionOptions.mode;
    _autoGainControlMode = autoGainControlOptions.mode;
    _highPassFilterMode = highPassFilterOptions.mode;
  }
  return self;
}

+ (instancetype)communicationOptions {
  return [[self alloc] initWithEchoCancellation:YES noiseSuppression:YES autoGainControl:YES highPassFilter:YES];
}

+ (instancetype)rawOptions {
  return [[self alloc] initWithEchoCancellation:NO noiseSuppression:NO autoGainControl:NO highPassFilter:NO];
}

@end

namespace {

RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode)
ObjCResultCode(webrtc::AudioProcessingOptionsResultCode code) {
  switch (code) {
    case webrtc::AudioProcessingOptionsResultCode::kApplied:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplied);
    case webrtc::AudioProcessingOptionsResultCode::kStored:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeStored);
    case webrtc::AudioProcessingOptionsResultCode::kRejectedRemoteTrack:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedRemoteTrack);
    case webrtc::AudioProcessingOptionsResultCode::kRejectedInvalidCombination:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedInvalidCombination);
    case webrtc::AudioProcessingOptionsResultCode::kRejectedPlatformUnavailable:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedPlatformUnavailable);
    case webrtc::AudioProcessingOptionsResultCode::kApplyFailed:
      return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplyFailed);
  }
  return RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplyFailed);
}

RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) *
    ResultWithCode(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode) code, NSString *message) {
  return [[RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) alloc] initWithCode:code message:message];
}

RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) * ResultFromNative(const webrtc::AudioProcessingOptionsResult &result) {
  NSString *message = result.message.empty() ? @"" : [NSString stringForStdString:result.message];
  return ResultWithCode(ObjCResultCode(result.code), message);
}

webrtc::AudioProcessingOptionsValidationContext AppleAudioProcessingValidationContext() {
  webrtc::AudioProcessingOptionsValidationContext context;
  context.topology =
      webrtc::AudioDeviceModule::PlatformAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled;
#if !TARGET_OS_SIMULATOR
  context.is_echo_cancellation_platform_available = true;
  context.is_noise_suppression_platform_available = true;
  context.is_auto_gain_control_platform_available = true;
  context.is_echo_noise_platform_path_available = true;
#endif
  return context;
}

webrtc::AudioProcessingOptionsResult ValidateAudioProcessingOptionsForFactory(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *
                                                                                  factory,
                                                                              const webrtc::AudioOptions &options) {
  webrtc::scoped_refptr<webrtc::AudioDeviceModule> adm = factory.nativeAudioDeviceModule;
  webrtc::Thread *workerThread = factory.workerThread;
  if (adm == nullptr || workerThread == nullptr) {
    return webrtc::ValidateAudioProcessingOptions(options, AppleAudioProcessingValidationContext());
  }

  auto validate = [adm, options] {
    return webrtc::ValidateAudioProcessingOptions(
        options, webrtc::AudioProcessingValidationContextForAudioDeviceModule(adm.get()));
  };
  if (workerThread->IsCurrent()) {
    return validate();
  }
  return workerThread->BlockingCall(validate);
}

}  // namespace

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

- (RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) *)setAudioProcessingOptions:
    (RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options {
  NSParameterAssert(options);
  if (!options) {
    return ResultWithCode(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplyFailed),
                          @"Audio processing options must not be nil");
  }
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->BlockingCall([self, options] { return [self setAudioProcessingOptions:options]; });
  }

  webrtc::AudioOptions nativeOptions = webrtc::objc::NativeAudioProcessingOptions(options);
  webrtc::AudioProcessingOptionsResult validation =
      ValidateAudioProcessingOptionsForFactory(self.factory, nativeOptions);
  if (!validation.ok()) {
    return ResultFromNative(validation);
  }

  return ResultFromNative(self.nativeAudioTrack->SetAudioProcessingOptions(nativeOptions));
}

#pragma mark - Private

- (webrtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return webrtc::scoped_refptr<webrtc::AudioTrackInterface>(
      static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get()));
}

@end
