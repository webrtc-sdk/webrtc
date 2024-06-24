/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoEncoderVP9.h"
#import "RTCWrappedNativeVideoEncoder.h"

#import "helpers/NSString+StdString.h"

#include "absl/container/inlined_vector.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/svc/create_scalability_structure.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderVP9)

+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)vp9Encoder {
  std::unique_ptr<webrtc::VideoEncoder> nativeEncoder(webrtc::VP9Encoder::Create());
  if (nativeEncoder == nullptr) {
    return nil;
  }
  return [[RTC_OBJC_TYPE(RTCWrappedNativeVideoEncoder) alloc]
      initWithNativeEncoder:std::move(nativeEncoder)];
}

+ (bool)isSupported {
#if defined(RTC_ENABLE_VP9)
  return true;
#else
  return false;
#endif
}

+ (NSArray<NSString *> *)scalabilityModes {
    NSMutableArray<NSString *> *scalabilityModes = [NSMutableArray array];
    for (const auto scalability_mode : webrtc::kAllScalabilityModes) {
      if (webrtc::ScalabilityStructureConfig(scalability_mode).has_value()) {
       [scalabilityModes addObject:[NSString stringForAbslStringView:webrtc::ScalabilityModeToString(scalability_mode)]];
      }
    }
    return scalabilityModes;
}

@end
