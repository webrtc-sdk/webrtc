/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecCapability+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCRtpCodecCapability)

@synthesize nativeCodecCapability = _nativeCodecCapability;

- (instancetype)init {
  webrtc::RtpCodecCapability nativeCodecCapability;
  return [self initWithNativeCodecCapability:nativeCodecCapability];
}

- (instancetype)initWithNativeCodecCapability:(const webrtc::RtpCodecCapability &)nativeCodecCapability {

  if (self = [super init]) {
    _nativeCodecCapability = nativeCodecCapability;
  }

  return self;
}

- (NSString *)mimeType {
  return [NSString stringWithUTF8String:_nativeCodecCapability.mime_type().c_str()];
}

- (NSString *)name {
  return [NSString stringWithUTF8String:_nativeCodecCapability.name.c_str()];
}

- (void)setName:(NSString *)name {
  _nativeCodecCapability.name = std::string([name UTF8String]);
}

@end
