/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

@class RTC_OBJC_TYPE(RTCRtpCodecCapability);

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCRtpCapabilities) : NSObject

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *codecs;

// Not implemented.
// std::vector<RtpHeaderExtensionCapability> header_extensions;

//Not implemented.
// std::vector<FecMechanism> fec;

@end

NS_ASSUME_NONNULL_END
