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

#import "RTCH265ProfileLevelId.h"

#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"

NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecH265Name) = @(webrtc::kH265CodecName);
NSString *const RTC_CONSTANT_TYPE(RTCH265ProfileMain) = @"4d001f";
