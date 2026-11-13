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

#import "RTCVideoFrame.h"
#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/** Represents an encoded frame's type. */
typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCFrameType)) {
  RTC_OBJC_TYPE(RTCFrameTypeEmptyFrame) = 0,
  RTC_OBJC_TYPE(RTCFrameTypeAudioFrameSpeech) = 1,
  RTC_OBJC_TYPE(RTCFrameTypeAudioFrameCN) = 2,
  RTC_OBJC_TYPE(RTCFrameTypeVideoFrameKey) = 3,
  RTC_OBJC_TYPE(RTCFrameTypeVideoFrameDelta) = 4,
};

typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCVideoContentType)) {
  RTC_OBJC_TYPE(RTCVideoContentTypeUnspecified),
  RTC_OBJC_TYPE(RTCVideoContentTypeScreenshare),
};

/** Represents an encoded frame. Corresponds to webrtc::EncodedImage. */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCEncodedImage) : NSObject

@property(nonatomic, strong) NSData *buffer;
@property(nonatomic, assign) int32_t encodedWidth;
@property(nonatomic, assign) int32_t encodedHeight;
@property(nonatomic, assign) uint32_t timeStamp;
@property(nonatomic, assign) int64_t captureTimeMs;
@property(nonatomic, assign) int64_t ntpTimeMs;
/** First packet receive time in milliseconds, or -1 if unknown.
 * Uses the same timebase as CACurrentMediaTime() * 1000. Comparable with a
 * valid renderTimeMs passed to RTCVideoDecoder, but not its 0 (low latency) or
 * -1 sentinel values, nor ntpTimeMs. For combined spatial layers, this is the
 * first layer's receive start time.
 */
@property(nonatomic, assign) int64_t receiveStartMs;
/** Last packet receive time in the same clock as receiveStartMs, or -1 if
 * unknown. For combined spatial layers, this is the last layer's receive finish
 * time.
 */
@property(nonatomic, assign) int64_t receiveFinishMs;
@property(nonatomic, assign) uint8_t flags;
@property(nonatomic, assign) int64_t encodeStartMs;
@property(nonatomic, assign) int64_t encodeFinishMs;
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCFrameType) frameType;
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCVideoRotation) rotation;
@property(nonatomic, strong) NSNumber *qp;
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCVideoContentType) contentType;

@end

NS_ASSUME_NONNULL_END
