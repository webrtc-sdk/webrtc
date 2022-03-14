/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCAudioDevice.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioDeviceModule) : NSObject

- (void)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer;

@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *outputDevices;
@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *inputDevices;

@property(nonatomic, readonly) BOOL playing;
@property(nonatomic, readonly) BOOL recording;

// Executes low-level API's in sequence to switch the device
- (BOOL)setOutputDevice: (nullable RTCAudioDevice *)device;
- (BOOL)setInputDevice: (nullable RTCAudioDevice *)device;

@end

NS_ASSUME_NONNULL_END
