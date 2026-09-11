/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoDecoder.h"

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCVideoDecoderH265) : NSObject <RTC_OBJC_TYPE(RTCVideoDecoder)>

/** When YES, streams whose decoder configuration signals a luma bit depth
 *  above 8 (e.g. HEVC Main10) are decoded to a 10-bit biplanar pixel format
 *  (kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange) so bit depth and HDR
 *  colorimetry survive decode. Defaults to NO, which keeps the historical
 *  behaviour of downconverting every stream to 8-bit NV12.
 *
 *  Only enable this when every consumer of the decoded frames handles
 *  10-bit biplanar output: RTCMTLVideoView's shaders and RTCCVPixelBuffer's
 *  I420 conversion currently assume 8-bit formats. Set before decoding
 *  starts; it is read when a decompression session is created.
 */
@property(class, nonatomic, assign) BOOL preferHighBitDepthOutput;

- (NSInteger)setHVCCFormat:(const uint8_t *)data size:(size_t)size width:(uint16_t)width height:(uint16_t)height;
- (NSInteger)decodeData:(const uint8_t *)data
    size:(size_t)size
    timeStamp:(int64_t)timeStamp;
- (void)flush;
@end