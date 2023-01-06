/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCapabilities+Private.h"
#import "RTCRtpCodecCapability+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCRtpCapabilities)

@synthesize nativeCapabilities = _nativeCapabilities;

- (instancetype)initWithNativeCapabilities:(const webrtc::RtpCapabilities &)nativeCapabilities {

  if (self = [super init]) {
    _nativeCapabilities = nativeCapabilities;
  }

  return self;
}

- (NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecs {

  NSMutableArray *result = [NSMutableArray array];

  for (auto & element : _nativeCapabilities.codecs) {
    RTCRtpCodecCapability *object = [[RTCRtpCodecCapability alloc] initWithNativeCodecCapability: element];
    [result addObject: object];
  }

  return result;
}

@end
