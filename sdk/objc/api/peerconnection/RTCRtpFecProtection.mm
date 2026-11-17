/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpFecProtection+Private.h"

namespace {

webrtc::RtpFecProtection::Mode NativeModeFromMode(
    RTC_OBJC_TYPE(RTCRtpFecProtectionMode) mode) {
  switch (mode) {
    case RTC_OBJC_TYPE(RTCRtpFecProtectionModeAdaptive):
      return webrtc::RtpFecProtection::Mode::kAdaptive;
    case RTC_OBJC_TYPE(RTCRtpFecProtectionModeFixed):
      return webrtc::RtpFecProtection::Mode::kFixed;
    case RTC_OBJC_TYPE(RTCRtpFecProtectionModeMinimum):
      return webrtc::RtpFecProtection::Mode::kMinimum;
    case RTC_OBJC_TYPE(RTCRtpFecProtectionModeDisabled):
      return webrtc::RtpFecProtection::Mode::kDisabled;
  }
  return webrtc::RtpFecProtection::Mode::kAdaptive;
}

RTC_OBJC_TYPE(RTCRtpFecProtectionMode)
ModeFromNativeMode(webrtc::RtpFecProtection::Mode mode) {
  switch (mode) {
    case webrtc::RtpFecProtection::Mode::kAdaptive:
      return RTC_OBJC_TYPE(RTCRtpFecProtectionModeAdaptive);
    case webrtc::RtpFecProtection::Mode::kFixed:
      return RTC_OBJC_TYPE(RTCRtpFecProtectionModeFixed);
    case webrtc::RtpFecProtection::Mode::kMinimum:
      return RTC_OBJC_TYPE(RTCRtpFecProtectionModeMinimum);
    case webrtc::RtpFecProtection::Mode::kDisabled:
      return RTC_OBJC_TYPE(RTCRtpFecProtectionModeDisabled);
  }
  return RTC_OBJC_TYPE(RTCRtpFecProtectionModeAdaptive);
}

}  // namespace

@implementation RTC_OBJC_TYPE (RTCRtpFecProtection)

@synthesize mode = _mode;
@synthesize protectionFactor = _protectionFactor;
@synthesize keyFrameProtectionFactor = _keyFrameProtectionFactor;
@synthesize maxFecFrames = _maxFecFrames;

- (instancetype)init {
  webrtc::RtpFecProtection nativeFecProtection;
  return [self initWithNativeFecProtection:nativeFecProtection];
}

- (instancetype)initWithNativeFecProtection:
    (const webrtc::RtpFecProtection &)nativeFecProtection {
  self = [super init];
  if (self) {
    _mode = ModeFromNativeMode(nativeFecProtection.mode);
    _protectionFactor = nativeFecProtection.protection_factor;
    if (nativeFecProtection.key_frame_protection_factor) {
      _keyFrameProtectionFactor =
          @(*nativeFecProtection.key_frame_protection_factor);
    }
    if (nativeFecProtection.max_fec_frames) {
      _maxFecFrames = @(*nativeFecProtection.max_fec_frames);
    }
  }
  return self;
}

- (webrtc::RtpFecProtection)nativeFecProtection {
  webrtc::RtpFecProtection fecProtection;
  fecProtection.mode = NativeModeFromMode(_mode);
  fecProtection.protection_factor = _protectionFactor;
  if (_keyFrameProtectionFactor != nil) {
    fecProtection.key_frame_protection_factor =
        _keyFrameProtectionFactor.doubleValue;
  }
  if (_maxFecFrames != nil) {
    fecProtection.max_fec_frames = _maxFecFrames.intValue;
  }
  return fecProtection;
}

@end
