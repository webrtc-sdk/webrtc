/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoCodecConstants.h"

#include "media/base/media_constants.h"

NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecVp8Name) = @(webrtc::kVp8CodecName);
NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecVp9Name) = @(webrtc::kVp9CodecName);
NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecAv1Name) = @(webrtc::kAv1CodecName);
