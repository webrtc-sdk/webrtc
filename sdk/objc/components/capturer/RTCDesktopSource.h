/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"

typedef NS_ENUM(NSInteger, RTCDesktopSourceType) {
  RTCDesktopSourceTypeScreen,
  RTCDesktopSourceTypeWindow,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCDesktopSource) : NSObject

@property(nonatomic, readonly) NSString *sourceId;

@property(nonatomic, readonly) NSString *name;

@property(nonatomic, readonly) NSImage *thumbnail;

@property(nonatomic, readonly) RTCDesktopSourceType sourceType;

@end