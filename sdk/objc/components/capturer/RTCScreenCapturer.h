/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCapturer.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(ScreenCapturerDelegate)<NSObject> -
    (void)didCaptureVideoFrame
    : (RTC_OBJC_TYPE(RTCVideoFrame) *)frame;
@end

RTC_OBJC_EXPORT
// Screen capture that implements RTCVideoCapturer. Delivers frames to a
// RTCVideoCapturerDelegate (usually RTCVideoSource).
@interface RTC_OBJC_TYPE (RTCScreenCapturer) : RTC_OBJC_TYPE(RTCVideoCapturer)

- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate;

// Starts the capture session asynchronously.
- (void)startCapture:(NSInteger)fps;
// Stops the capture session asynchronously.
- (void)stopCapture;

- (void)didCaptureVideoFrame
    : (RTC_OBJC_TYPE(RTCVideoFrame) *)frame;

@end

NS_ASSUME_NONNULL_END
