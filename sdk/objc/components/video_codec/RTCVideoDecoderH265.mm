/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoDecoderH265.h"

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Decoder.
@implementation RTC_OBJC_TYPE (RTCVideoDecoderH265) {
}

- (instancetype)init {
  self = [super init];
  if (self) {
  }
  return self;
}

- (void)dealloc {
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)releaseDecoder {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end
