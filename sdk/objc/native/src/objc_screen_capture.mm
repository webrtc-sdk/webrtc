#include "sdk/objc/native/src/objc_screen_capture.h"
#include "sdk/objc/native/src/objc_video_frame.h"

#include "third_party/libyuv/include/libyuv.h"

#import "components/capturer/RTCScreenCapturer.h"

namespace webrtc {

enum { kCaptureDelay = 33, kCaptureMessageId = 1000 };

ObjCScreenCapture::ObjCScreenCapture(id<RTC_OBJC_TYPE(ScreenCapturerDelegate)> delegate)
 : thread_(rtc::Thread::Create()), delegate_(delegate) {
  webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();
  capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
  thread_->Start();
}

ObjCScreenCapture::~ObjCScreenCapture() {
  thread_->Stop();
}

CaptureState ObjCScreenCapture::Start() {
  capture_state_ = CS_RUNNING;
  capturer_->Start(this);
  CaptureFrame();
  return CS_RUNNING;
}

void ObjCScreenCapture::Stop() {
  capture_state_ = CS_STOPPED;
}

bool ObjCScreenCapture::IsRunning() {
  return capture_state_ == CS_RUNNING;
}

void ObjCScreenCapture::OnCaptureResult(webrtc::DesktopCapturer::Result result,
                                        std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }
  int width = frame->size().width();
  int height = frame->size().height();
  if (!i420_buffer_ || !i420_buffer_.get() ||
      i420_buffer_->width() * i420_buffer_->height() < width * height) {
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

   RTCVideoFrame *rtc_video_frame = ToObjCVideoFrame(webrtc::VideoFrame(i420_buffer_, 0, 0, webrtc::kVideoRotation_0));
   [delegate_ didCaptureVideoFrame:rtc_video_frame];
}

void ObjCScreenCapture::OnMessage(rtc::Message* msg) {
  if (msg->message_id == kCaptureMessageId) {
    CaptureFrame();
  }
}

void ObjCScreenCapture::CaptureFrame() {
  if (capture_state_ == CS_RUNNING) {
    capturer_->CaptureFrame();
    thread_->PostDelayed(RTC_FROM_HERE, kCaptureDelay, this, kCaptureMessageId);
  }
}

}  // namespace webrtc
