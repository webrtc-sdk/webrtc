/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo+Private.h"

#import "helpers/NSString+StdString.h"

#include "absl/container/inlined_vector.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/svc/create_scalability_structure.h"

@implementation RTC_OBJC_TYPE (RTCVideoCodecInfo)
(Private)

    - (instancetype)initWithNativeSdpVideoFormat : (webrtc::SdpVideoFormat)format {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = format.parameters.begin(); it != format.parameters.end(); ++it) {
    [params setObject:[NSString stringForStdString:it->second]
               forKey:[NSString stringForStdString:it->first]];
  }
  return [self initWithName:[NSString stringForStdString:format.name] parameters:params];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in self.parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:self.parameters[paramKey]];
    parameters[key] = value;
  }
    
 absl::InlinedVector<webrtc::ScalabilityMode, webrtc::kScalabilityModeCount>
    scalability_modes;
  for (NSString *scalabilityMode in self.scalabilityModes) {
    auto scalability_mode = webrtc::ScalabilityModeFromString([NSString stdStringForString:scalabilityMode]);
    if (scalability_mode != absl::nullopt) {
      scalability_modes.push_back(*scalability_mode);
    }
  }
  return webrtc::SdpVideoFormat([NSString stdStringForString:self.name], parameters, scalability_modes);
}

@end
