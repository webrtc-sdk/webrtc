/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_CAPTURE_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_CAPTURE_H_

#import "base/RTCMacros.h"

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/thread.h"

@protocol RTC_OBJC_TYPE
(DesktopCapturerDelegate);

namespace webrtc {

class ObjCDesktopCapturer : public DesktopCapturer::Callback, public rtc::MessageHandler {
 public:
  enum CaptureState { CS_RUNNING, CS_STOPPED, CS_FAILED};

  enum DesktopType { kScreen, kWindow };

 public:
  ObjCDesktopCapturer(DesktopType type,
    webrtc::DesktopCapturer::SourceId source_id, 
    id<RTC_OBJC_TYPE(DesktopCapturerDelegate)> delegate);
  virtual ~ObjCDesktopCapturer();

  virtual CaptureState Start();

  virtual void Stop();

  virtual bool IsRunning();

 protected:
  virtual void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame) override;
  virtual void OnMessage(rtc::Message* msg) override;

 private:
  void CaptureFrame();
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  std::unique_ptr<rtc::Thread> thread_;
  rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer_;
  CaptureState capture_state_ = CS_STOPPED;
  webrtc::DesktopCapturer::SourceId source_id_;
  id<RTC_OBJC_TYPE(DesktopCapturerDelegate)> delegate_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_CAPTURE_H_
