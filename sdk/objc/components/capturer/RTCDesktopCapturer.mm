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