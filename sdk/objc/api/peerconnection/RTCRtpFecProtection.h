/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/** Corresponds to webrtc::RtpFecProtection::Mode. */
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCRtpFecProtectionMode)) {
  /** Protection is derived from the observed loss and RTT. */
  RTC_OBJC_TYPE(RTCRtpFecProtectionModeAdaptive),
  /** `protectionFactor` is used regardless of loss and RTT. */
  RTC_OBJC_TYPE(RTCRtpFecProtectionModeFixed),
  /** Adaptive, but never below `protectionFactor`. */
  RTC_OBJC_TYPE(RTCRtpFecProtectionModeMinimum),
  /** No FEC packets are generated even if FEC was negotiated. */
  RTC_OBJC_TYPE(RTCRtpFecProtectionModeDisabled),
};

/**
 * Non-standard sender-wide FEC protection policy. Corresponds to
 * webrtc::RtpFecProtection. Has no effect unless FEC was negotiated, and only
 * applies to video senders.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCRtpFecProtection) : NSObject

@property(nonatomic, assign) RTC_OBJC_TYPE(RTCRtpFecProtectionMode) mode;

/**
 * Number of FEC packets relative to the number of media packets, in
 * [0.0, 1.0]. Used by the fixed and minimum modes.
 */
@property(nonatomic, assign) double protectionFactor;

/** Protection factor for key frames. If nil, `protectionFactor` is used. */
@property(nonatomic, copy, nullable) NSNumber *keyFrameProtectionFactor;

/** Maximum number of frames covered by one FEC block, in [1, 6]. */
@property(nonatomic, copy, nullable) NSNumber *maxFecFrames;

- (instancetype)init;

@end

NS_ASSUME_NONNULL_END
