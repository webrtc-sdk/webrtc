/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaSource+Private.h"

#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCMediaSource) {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) * _factory;
  RTC_OBJC_TYPE(RTCMediaSourceType) _type;
}

@synthesize nativeMediaSource = _nativeMediaSource;

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
              nativeMediaSource:(webrtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource
                           type:(RTC_OBJC_TYPE(RTCMediaSourceType))type {
  RTC_DCHECK(factory);
  RTC_DCHECK(nativeMediaSource);
  self = [super init];
  if (self) {
    _factory = factory;
    _nativeMediaSource = nativeMediaSource;
    _type = type;
  }
  return self;
}

- (RTC_OBJC_TYPE(RTCSourceState))state {
  return [[self class] sourceStateForNativeState:_nativeMediaSource->state()];
}

#pragma mark - Private

+ (webrtc::MediaSourceInterface::SourceState)nativeSourceStateForState:
    (RTC_OBJC_TYPE(RTCSourceState))state {
  switch (state) {
    case RTC_OBJC_TYPE(RTCSourceStateInitializing):
      return webrtc::MediaSourceInterface::kInitializing;
    case RTC_OBJC_TYPE(RTCSourceStateLive):
      return webrtc::MediaSourceInterface::kLive;
    case RTC_OBJC_TYPE(RTCSourceStateEnded):
      return webrtc::MediaSourceInterface::kEnded;
    case RTC_OBJC_TYPE(RTCSourceStateMuted):
      return webrtc::MediaSourceInterface::kMuted;
  }
}

+ (RTC_OBJC_TYPE(RTCSourceState))sourceStateForNativeState:
    (webrtc::MediaSourceInterface::SourceState)nativeState {
  switch (nativeState) {
    case webrtc::MediaSourceInterface::kInitializing:
      return RTC_OBJC_TYPE(RTCSourceStateInitializing);
    case webrtc::MediaSourceInterface::kLive:
      return RTC_OBJC_TYPE(RTCSourceStateLive);
    case webrtc::MediaSourceInterface::kEnded:
      return RTC_OBJC_TYPE(RTCSourceStateEnded);
    case webrtc::MediaSourceInterface::kMuted:
      return RTC_OBJC_TYPE(RTCSourceStateMuted);
  }
}

+ (NSString *)stringForState:(RTC_OBJC_TYPE(RTCSourceState))state {
  switch (state) {
    case RTC_OBJC_TYPE(RTCSourceStateInitializing):
      return @"Initializing";
    case RTC_OBJC_TYPE(RTCSourceStateLive):
      return @"Live";
    case RTC_OBJC_TYPE(RTCSourceStateEnded):
      return @"Ended";
    case RTC_OBJC_TYPE(RTCSourceStateMuted):
      return @"Muted";
  }
}

@end
