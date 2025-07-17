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

#import "RTCVideoDecoderFactoryH265.h"

#import "RTCVideoDecoderH265.h"
#import "api/video_codec/RTCVideoCodecConstants.h"
#import "base/RTCVideoCodecInfo.h"

@implementation RTC_OBJC_TYPE (RTCVideoDecoderFactoryH265)

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *codecs =
      [NSMutableArray array];
  NSString *codecName = RTC_CONSTANT_TYPE(RTCVideoCodecH265Name);

  RTC_OBJC_TYPE(RTCVideoCodecInfo) *h265Info =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc]
          initWithName:codecName
            parameters:@{}];
  [codecs addObject:h265Info];

  return [codecs copy];
}

- (id<RTC_OBJC_TYPE(RTCVideoDecoder)>)createDecoder:
    (RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  return [[RTC_OBJC_TYPE(RTCVideoDecoderH265) alloc] init];
}

@end

#endif  // RTC_ENABLE_H265 