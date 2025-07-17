/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "sdk/objc/base/RTCMacros.h"

#ifdef RTC_ENABLE_H265

RTC_OBJC_EXPORT extern NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecH265Name);
RTC_OBJC_EXPORT extern NSString *const RTC_CONSTANT_TYPE(RTCLevel31MainTier);
RTC_OBJC_EXPORT extern NSString *const RTC_CONSTANT_TYPE(RTCMaxSupportedH265ProfileTierLevel);

/** H265 Profiles, Tiers, and Levels. */
typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCH265Profile)) {
  RTC_OBJC_TYPE(RTCH265ProfileMain),
  RTC_OBJC_TYPE(RTCH265ProfileMain10),
  RTC_OBJC_TYPE(RTCH265ProfileMainStill),
  RTC_OBJC_TYPE(RTCH265ProfileRangeExtensions),
  RTC_OBJC_TYPE(RTCH265ProfileHighThroughput),
  RTC_OBJC_TYPE(RTCH265ProfileScreenExtended),
  RTC_OBJC_TYPE(RTCH265ProfileScalableMain),
  RTC_OBJC_TYPE(RTCH265ProfileScalableMain10),
  RTC_OBJC_TYPE(RTCH265Profile3dMain),
};

typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCH265Tier)) {
  RTC_OBJC_TYPE(RTCH265TierMain),
  RTC_OBJC_TYPE(RTCH265TierHigh),
};

typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCH265Level)) {
  RTC_OBJC_TYPE(RTCH265Level1) = 30,
  RTC_OBJC_TYPE(RTCH265Level2) = 60,
  RTC_OBJC_TYPE(RTCH265Level2_1) = 63,
  RTC_OBJC_TYPE(RTCH265Level3) = 90,
  RTC_OBJC_TYPE(RTCH265Level3_1) = 93,
  RTC_OBJC_TYPE(RTCH265Level4) = 120,
  RTC_OBJC_TYPE(RTCH265Level4_1) = 123,
  RTC_OBJC_TYPE(RTCH265Level5) = 150,
  RTC_OBJC_TYPE(RTCH265Level5_1) = 153,
  RTC_OBJC_TYPE(RTCH265Level5_2) = 156,
  RTC_OBJC_TYPE(RTCH265Level6) = 180,
  RTC_OBJC_TYPE(RTCH265Level6_1) = 183,
  RTC_OBJC_TYPE(RTCH265Level6_2) = 186,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCH265ProfileTierLevel) : NSObject

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCH265Profile) profile;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCH265Tier) tier;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCH265Level) level;

- (instancetype)initWithProfile:(RTC_OBJC_TYPE(RTCH265Profile))profile
                           tier:(RTC_OBJC_TYPE(RTCH265Tier))tier
                          level:(RTC_OBJC_TYPE(RTCH265Level))level;

@end

#endif  // RTC_ENABLE_H265 