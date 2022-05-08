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

#import "RTCScreenCapturer.h"
#import "base/RTCLogging.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#include "sdk/objc/native/src/objc_screen_capture.h"


@implementation RTC_OBJC_TYPE (RTCScreenCapturer) {
    std::unique_ptr<webrtc::ObjCScreenCapture> capturer_;
}

// This initializer is used for testing.
- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate{
  if (self = [super initWithDelegate:delegate]) {
      capturer_ = std::make_unique<webrtc::ObjCScreenCapture>(self);
  }
  return self;
}

-(void)dealloc {
    capturer_->Stop();
    capturer_ = nullptr;
}

- (void)startCapture:(NSInteger)fps {
    capturer_->Start();
}

- (void)didCaptureVideoFrame
    : (RTC_OBJC_TYPE(RTCVideoFrame) *)frame {
        [self.delegate capturer:self didCaptureVideoFrame:frame];
}

- (void)stopCapture {
    capturer_->Stop();
}

@end
