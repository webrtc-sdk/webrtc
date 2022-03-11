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

RTC_OBJC_EXPORT
@interface RTCDevice : NSObject

+ (instancetype)defaultDevice;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, copy, readonly) NSString *guid;
@property(nonatomic, copy, readonly) NSString *name;

@end

NS_ASSUME_NONNULL_END
