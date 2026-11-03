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
#import <XCTest/XCTest.h>

#include "test/gtest.h"

#import "api/peerconnection/RTCRtpFecProtection+Private.h"
#import "api/peerconnection/RTCRtpParameters+Private.h"

@interface RTCRtpParametersTest : XCTestCase
@end

@implementation RTCRtpParametersTest

- (void)testFecProtectionIsNilByDefault {
  RTC_OBJC_TYPE(RTCRtpParameters) *parameters =
      [[RTC_OBJC_TYPE(RTCRtpParameters) alloc] init];
  XCTAssertNil(parameters.fecProtection);
  EXPECT_FALSE(parameters.nativeParameters.fec_protection.has_value());
}

- (void)testFecProtectionToNative {
  RTC_OBJC_TYPE(RTCRtpFecProtection) *fecProtection =
      [[RTC_OBJC_TYPE(RTCRtpFecProtection) alloc] init];
  fecProtection.mode = RTC_OBJC_TYPE(RTCRtpFecProtectionModeMinimum);
  fecProtection.protectionFactor = 0.25;
  fecProtection.keyFrameProtectionFactor = @0.5;
  fecProtection.maxFecFrames = @3;
  RTC_OBJC_TYPE(RTCRtpParameters) *parameters =
      [[RTC_OBJC_TYPE(RTCRtpParameters) alloc] init];
  parameters.fecProtection = fecProtection;

  webrtc::RtpParameters nativeParameters = parameters.nativeParameters;
  ASSERT_TRUE(nativeParameters.fec_protection.has_value());
  EXPECT_EQ(webrtc::RtpFecProtection::Mode::kMinimum,
            nativeParameters.fec_protection->mode);
  EXPECT_EQ(0.25, nativeParameters.fec_protection->protection_factor);
  EXPECT_EQ(0.5, nativeParameters.fec_protection->key_frame_protection_factor);
  EXPECT_EQ(3, nativeParameters.fec_protection->max_fec_frames);
}

- (void)testFecProtectionFromNative {
  webrtc::RtpParameters nativeParameters;
  webrtc::RtpFecProtection nativeFecProtection;
  nativeFecProtection.mode = webrtc::RtpFecProtection::Mode::kFixed;
  nativeFecProtection.protection_factor = 0.1;
  nativeParameters.fec_protection = nativeFecProtection;

  RTC_OBJC_TYPE(RTCRtpParameters) *parameters =
      [[RTC_OBJC_TYPE(RTCRtpParameters) alloc]
          initWithNativeParameters:nativeParameters];
  XCTAssertNotNil(parameters.fecProtection);
  XCTAssertEqual(RTC_OBJC_TYPE(RTCRtpFecProtectionModeFixed),
                 parameters.fecProtection.mode);
  XCTAssertEqual(0.1, parameters.fecProtection.protectionFactor);
  XCTAssertNil(parameters.fecProtection.keyFrameProtectionFactor);
  XCTAssertNil(parameters.fecProtection.maxFecFrames);
  EXPECT_EQ(nativeParameters.fec_protection,
            parameters.nativeParameters.fec_protection);
}

@end
