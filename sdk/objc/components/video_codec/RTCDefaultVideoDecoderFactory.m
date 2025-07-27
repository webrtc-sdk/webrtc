/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDefaultVideoDecoderFactory.h"

#import "RTCH264ProfileLevelId.h"
#import "RTCVideoDecoderH264.h"
#import "api/video_codec/RTCVideoCodecConstants.h"
#import "api/video_codec/RTCVideoDecoderVP8.h"
#import "api/video_codec/RTCVideoDecoderVP9.h"
#import "RTCH265ProfileLevelId.h"
#import "RTCVideoDecoderH265.h"
#import "base/RTCVideoCodecInfo.h"

#if defined(RTC_DAV1D_IN_INTERNAL_DECODER_FACTORY)
#import "api/video_codec/RTCVideoDecoderAV1.h"  // nogncheck
#endif

@implementation RTC_OBJC_TYPE (RTCDefaultVideoDecoderFactory)

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : RTC_CONSTANT_TYPE(RTCMaxSupportedH264ProfileLevelConstrainedHigh),
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedHighInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecH264Name)
                                                  parameters:constrainedHighParams];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : RTC_CONSTANT_TYPE(RTCMaxSupportedH264ProfileLevelConstrainedBaseline),
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedBaselineInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecH264Name)
                                                  parameters:constrainedBaselineParams];

  RTC_OBJC_TYPE(RTCVideoCodecInfo) *h265Info =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecH265Name)];

  RTC_OBJC_TYPE(RTCVideoCodecInfo) *vp8Info =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecVp8Name)];

  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *result = [@[
    constrainedHighInfo,
    constrainedBaselineInfo,
    vp8Info,
    h265Info,
  ] mutableCopy];

  if ([RTC_OBJC_TYPE(RTCVideoDecoderVP9) isSupported]) {
    [result
        addObject:[[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecVp9Name)]];
  }

#if defined(RTC_DAV1D_IN_INTERNAL_DECODER_FACTORY)
  [result addObject:[[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:RTC_CONSTANT_TYPE(RTCVideoCodecAv1Name)]];
#endif

  return result;
}

- (id<RTC_OBJC_TYPE(RTCVideoDecoder)>)createDecoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  if ([info.name isEqualToString:RTC_CONSTANT_TYPE(RTCVideoCodecH264Name)]) {
    return [[RTC_OBJC_TYPE(RTCVideoDecoderH264) alloc] init];
  } else if ([info.name isEqualToString:RTC_CONSTANT_TYPE(RTCVideoCodecVp8Name)]) {
    return [RTC_OBJC_TYPE(RTCVideoDecoderVP8) vp8Decoder];
  } else if ([info.name isEqualToString:RTC_CONSTANT_TYPE(RTCVideoCodecH265Name)]) {
    return [[RTC_OBJC_TYPE(RTCVideoDecoderH265) alloc] init];
  } else if ([info.name isEqualToString:RTC_CONSTANT_TYPE(RTCVideoCodecVp9Name)] &&
             [RTC_OBJC_TYPE(RTCVideoDecoderVP9) isSupported]) {
    return [RTC_OBJC_TYPE(RTCVideoDecoderVP9) vp9Decoder];
  }

#if defined(RTC_DAV1D_IN_INTERNAL_DECODER_FACTORY)
  if ([info.name isEqualToString:RTC_CONSTANT_TYPE(RTCVideoCodecAv1Name)]) {
    return [RTC_OBJC_TYPE(RTCVideoDecoderAV1) av1Decoder];
  }
#endif

  return nil;
}

@end
