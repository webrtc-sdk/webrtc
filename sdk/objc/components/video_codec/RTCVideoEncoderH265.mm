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

#import "RTCVideoEncoderH265.h"

#import <VideoToolbox/VideoToolbox.h>
#include <vector>

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif
#import "RTCCodecSpecificInfoH265.h"
#import "RTCH265ProfileLevelId.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"
#import "base/RTCCodecSpecificInfo.h"
#import "base/RTCI420Buffer.h"
#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"

#include "api/video_codecs/h265_profile_level_id.h"
#include "common_video/h265/h265_bitstream_parser.h"
#include "common_video/include/bitrate_adjuster.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

namespace {  // anonymous namespace

@implementation RTC_OBJC_TYPE (RTCVideoEncoderH265) {
}

- (instancetype)initWithCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)codecInfo {
  self = [super init];
  if (self) {
  }
  return self;
}

- (void)dealloc {
  [self destroyCompressionSession];
}

- (NSInteger)startEncodeWithSettings:(RTC_OBJC_TYPE(RTCVideoEncoderSettings) *)settings
                       numberOfCores:(int)numberOfCores {
  return [self resetCompressionSessionWithPixelFormat:kNV12PixelFormat];
}

- (NSInteger)encode:(RTC_OBJC_TYPE(RTCVideoFrame) *)frame
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
  _callback = callback;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)resolutionAlignment {
  return 1;
}

- (BOOL)applyAlignmentToAllSimulcastLayers {
  return NO;
}

- (BOOL)supportsNativeHandle {
  return YES;
}

#pragma mark - Private

- (NSInteger)releaseEncoder {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

- (nullable RTC_OBJC_TYPE(RTCVideoEncoderQpThresholds) *)scalingSettings {
  return nil;
}

@end
