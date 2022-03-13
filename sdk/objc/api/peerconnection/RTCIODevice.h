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

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCIODeviceType) {
  RTCIODeviceTypeOutput,
  RTCIODeviceTypeInput,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE(RTCIODevice) : NSObject

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) BOOL isDefault;
@property(nonatomic, readonly) RTCIODeviceType type;
@property(nonatomic, copy, readonly) NSString *deviceId;
@property(nonatomic, copy, readonly) NSString *name;

@end

NS_ASSUME_NONNULL_END
