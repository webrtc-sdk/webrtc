/*
 * Copyright 2023 LiveKit
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

  for (auto &element : _nativeCapabilities.codecs) {
    RTCRtpCodecCapability *object =
        [[RTCRtpCodecCapability alloc] initWithNativeCodecCapability:element];
    [result addObject:object];
  }

  return result;
}

@end
