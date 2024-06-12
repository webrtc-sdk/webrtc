/*
 * Copyright 2022 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <Foundation/Foundation.h>

#import "base/RTCLogging.h"
#import "base/RTCVideoFrameBuffer.h"

#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#import "RTCDesktopCapturer.h"
#import "RTCDesktopCapturer+Private.h"
#import "RTCDesktopSource+Private.h"

@implementation RTC_OBJC_TYPE (RTCDesktopCapturer) {
    __weak id<RTC_OBJC_TYPE(RTCDesktopCapturerDelegate)> _delegate;
}

@synthesize nativeCapturer = _nativeCapturer;
@synthesize source = _source;

- (instancetype)initWithSource:(RTC_OBJC_TYPE(RTCDesktopSource) *)source delegate:(__weak id<RTC_OBJC_TYPE(RTCDesktopCapturerDelegate)>)delegate captureDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)captureDelegate {
    if (self = [super initWithDelegate:captureDelegate]) {
      webrtc::DesktopType captureType = webrtc::kScreen;
      if(source.sourceType == RTCDesktopSourceTypeWindow) {
          captureType = webrtc::kWindow;
      }
      _nativeCapturer = std::make_shared<webrtc::ObjCDesktopCapturer>(captureType, source.nativeMediaSource->id(), self);
      _source = source;
      _delegate = delegate;
  }
  return self;
}

- (instancetype)initWithDefaultScreen:(__weak id<RTC_OBJC_TYPE(RTCDesktopCapturerDelegate)>)delegate captureDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)captureDelegate {
    if (self = [super initWithDelegate:captureDelegate]) {
      _nativeCapturer = std::make_unique<webrtc::ObjCDesktopCapturer>(webrtc::kScreen, -1, self);
      _source = nil;
      _delegate = delegate;
  }
  return self;
}


-(void)dealloc {
    _nativeCapturer->Stop();
    _nativeCapturer = nullptr;
}

- (void)startCapture {
    [self didSourceCaptureStart];
    _nativeCapturer->Start(30);
}

- (void)startCaptureWithFPS:(NSInteger)fps {
    _nativeCapturer->Start(fps);
}

- (void)didCaptureVideoFrame
    : (RTC_OBJC_TYPE(RTCVideoFrame) *)frame {
        [self.delegate capturer:self didCaptureVideoFrame:frame];
}

- (void)stopCapture {
    _nativeCapturer->Stop();
}

- (void)stopCaptureWithCompletionHandler:(nullable void (^)(void))completionHandler {
    [self stopCapture];
    if(completionHandler != nil) {
        completionHandler();
    }
}

-(void)didSourceCaptureStart {
    [_delegate didSourceCaptureStart:self];
}

-(void)didSourceCapturePaused {
   [_delegate didSourceCapturePaused:self];
}

-(void)didSourceCaptureStop {
    [_delegate didSourceCaptureStop:self];
}

-(void)didSourceCaptureError {
   [_delegate didSourceCaptureError:self];
}

@end
