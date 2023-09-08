/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
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
#import "RTCVideoEncoderAV1.h"
#import "RTCWrappedNativeVideoEncoder.h"

#import "helpers/NSString+StdString.h"

#include "absl/container/inlined_vector.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1)

+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)av1Encoder {
  std::unique_ptr<webrtc::VideoEncoder> nativeEncoder(webrtc::CreateLibaomAv1Encoder());
  return [[RTC_OBJC_TYPE(RTCWrappedNativeVideoEncoder) alloc]
      initWithNativeEncoder:std::move(nativeEncoder)];
}

+ (bool)isSupported {
  return true;
}

+ (NSArray<NSString *> *)scalabilityModes {
    absl::InlinedVector<webrtc::ScalabilityMode, webrtc::kScalabilityModeCount>
        scalability_modes = webrtc::LibaomAv1EncoderSupportedScalabilityModes();
    NSMutableArray<NSString *> *scalabilityModes = [NSMutableArray array];
    for (const webrtc::ScalabilityMode &scalability_mode : scalability_modes) {
      [scalabilityModes addObject:[NSString stringForAbslStringView:webrtc::ScalabilityModeToString(scalability_mode)]];
    }
    return scalabilityModes;
}

@end
