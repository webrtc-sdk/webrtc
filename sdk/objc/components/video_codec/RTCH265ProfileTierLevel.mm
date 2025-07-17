/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef RTC_ENABLE_H265

#import "RTCH265ProfileTierLevel.h"

#include "media/base/media_constants.h"

NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecH265Name) = @(webrtc::kH265CodecName);
NSString *const RTC_CONSTANT_TYPE(RTCLevel31MainTier) = @"level3.1-main-tier";
NSString *const RTC_CONSTANT_TYPE(RTCMaxSupportedH265ProfileTierLevel) = @"level5.2-main-tier";

@implementation RTC_OBJC_TYPE (RTCH265ProfileTierLevel)

@synthesize profile = _profile;
@synthesize tier = _tier;
@synthesize level = _level;

- (instancetype)initWithProfile:(RTC_OBJC_TYPE(RTCH265Profile))profile
                           tier:(RTC_OBJC_TYPE(RTCH265Tier))tier
                          level:(RTC_OBJC_TYPE(RTCH265Level))level {
  if (self = [super init]) {
    _profile = profile;
    _tier = tier;
    _level = level;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[RTC_OBJC_TYPE(RTCH265ProfileTierLevel) class]]) {
    return NO;
  }
  RTC_OBJC_TYPE(RTCH265ProfileTierLevel) *profile = object;
  return self.profile == profile.profile && self.tier == profile.tier && self.level == profile.level;
}

- (NSUInteger)hash {
  return (NSUInteger)(self.profile ^ self.tier ^ self.level);
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTC_OBJC_TYPE(RTCH265ProfileTierLevel) profile:%lu tier:%lu level:%lu",
                                    (unsigned long)self.profile, (unsigned long)self.tier, (unsigned long)self.level];
}

@end

#endif  // RTC_ENABLE_H265 