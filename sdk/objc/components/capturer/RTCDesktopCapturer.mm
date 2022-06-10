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

#import "base/RTCLogging.h"
#import "base/RTCVideoFrameBuffer.h"

#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#import "RTCDesktopCapturer.h"
#import "RTCDesktopCapturer+Private.h"
#import "RTCDesktopSource+Private.h"

@implementation RTC_OBJC_TYPE (RTCDesktopCapturer) {
}

@synthesize nativeCapturer = _nativeCapturer;
@synthesize source = _source;

- (instancetype)initWithSource:(RTCDesktopSource*)source delegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate {
    if (self = [super initWithDelegate:delegate]) {
      webrtc::ObjCDesktopCapturer::DesktopType captureType = webrtc::ObjCDesktopCapturer::kScreen;
      if(source.sourceType == RTCDesktopSourceTypeWindow) {
          captureType = webrtc::ObjCDesktopCapturer::kWindow;
      }
      _nativeCapturer = std::make_shared<webrtc::ObjCDesktopCapturer>(captureType, source.nativeMediaSource->id(), self);
      _source = source;
  }
  return self;
}

- (instancetype)initWithDefaultScreen:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate {
    if (self = [super initWithDelegate:delegate]) {
      _nativeCapturer = std::make_unique<webrtc::ObjCDesktopCapturer>(webrtc::ObjCDesktopCapturer::kScreen, -1, self);
      _source = nil;
  }
  return self;
}


-(void)dealloc {
    _nativeCapturer->Stop();
    _nativeCapturer = nullptr;
}

- (void)startCapture:(NSInteger)fps {
    _nativeCapturer->Start();
}

- (void)didCaptureVideoFrame
    : (RTC_OBJC_TYPE(RTCVideoFrame) *)frame {
        [self.delegate capturer:self didCaptureVideoFrame:frame];
}

- (void)stopCapture {
    _nativeCapturer->Stop();
}

@end
