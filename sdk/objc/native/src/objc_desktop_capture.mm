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

#include "sdk/objc/native/src/objc_desktop_capture.h"
#include "sdk/objc/native/src/objc_video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

#import "components/capturer/RTCDesktopCapturer+Private.h"

namespace webrtc {

enum { kCaptureDelay = 33, kCaptureMessageId = 1000 };

ObjCDesktopCapturer::ObjCDesktopCapturer(DesktopType type,
                                     webrtc::DesktopCapturer::SourceId source_id, 
                                     id<RTC_OBJC_TYPE(DesktopCapturerDelegate)> delegate)
    : thread_(rtc::Thread::Create()), source_id_(source_id), delegate_(delegate) {
  options_ = webrtc::DesktopCaptureOptions::CreateDefault();
  options_.set_detect_updated_region(true);
  options_.set_allow_iosurface(true);
  if (type == kScreen) {
    capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options_);
  }
  else { capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options_); }
  type_ = type;
  thread_->Start();
}

ObjCDesktopCapturer::~ObjCDesktopCapturer() {
  thread_->Stop();
}

ObjCDesktopCapturer::CaptureState ObjCDesktopCapturer::Start() {
  if(source_id_ != -1) {
    if(!capturer_->SelectSource(source_id_) && (type_ == kWindow && !capturer_->FocusOnSelectedSource())) {
        capture_state_ = CS_FAILED;
        return capture_state_;
    }
  }
  capturer_->Start(this);
  capture_state_ = CS_RUNNING;
  CaptureFrame();
  return capture_state_;
}

void ObjCDesktopCapturer::Stop() {
  capture_state_ = CS_STOPPED;
}

bool ObjCDesktopCapturer::IsRunning() {
  return capture_state_ == CS_RUNNING;
}

void ObjCDesktopCapturer::OnCaptureResult(webrtc::DesktopCapturer::Result result,
                                        std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }
  int width = frame->size().width();
  int height = frame->size().height();
  if (!i420_buffer_ || !i420_buffer_.get() ||
      i420_buffer_->width() * i420_buffer_->height() != width * height) {
    i420_buffer_ = webrtc::I420Buffer::Create(width, height);
  }
  libyuv::ConvertToI420(frame->data(),
                        0,
                        i420_buffer_->MutableDataY(),
                        i420_buffer_->StrideY(),
                        i420_buffer_->MutableDataU(),
                        i420_buffer_->StrideU(),
                        i420_buffer_->MutableDataV(),
                        i420_buffer_->StrideV(),
                        0,
                        0,
                        width,
                        height,
                        width,
                        height,
                        libyuv::kRotate0,
                        libyuv::FOURCC_ARGB);

  RTCVideoFrame* rtc_video_frame =
      ToObjCVideoFrame(webrtc::VideoFrame(i420_buffer_, 0, 0, webrtc::kVideoRotation_0));
  [delegate_ didCaptureVideoFrame:rtc_video_frame];
}

void ObjCDesktopCapturer::OnMessage(rtc::Message* msg) {
  if (msg->message_id == kCaptureMessageId) {
    CaptureFrame();
  }
}

void ObjCDesktopCapturer::CaptureFrame() {
  if (capture_state_ == CS_RUNNING) {
    capturer_->CaptureFrame();
    thread_->PostDelayed(RTC_FROM_HERE, kCaptureDelay, this, kCaptureMessageId);
  }
}

}  // namespace webrtc
