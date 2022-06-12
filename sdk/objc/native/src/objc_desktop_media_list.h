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

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_MEDIA_LIST_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_MEDIA_LIST_H_

#import "base/RTCMacros.h"

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/thread.h"

#import "components/capturer/RTCDesktopMediaList+Private.h"

namespace webrtc {

class ObjCDesktopMediaList : public rtc::MessageHandler {
public:
    enum DesktopType { kScreen, kWindow };

    class DesktopMediaListObserver {
    public:
        virtual void OnSourceAdded(int index) = 0;
        virtual void OnSourceRemoved(int index) = 0;
        virtual void OnSourceMoved(int old_index, int new_index) = 0;
        virtual void OnSourceNameChanged(int index) = 0;
        virtual void OnSourceThumbnailChanged(int index) = 0;
        virtual void OnSourcePreviewChanged(size_t index) = 0;

    protected:
        virtual ~DesktopMediaListObserver() {}
    };

    class MediaSource {
    public:
        MediaSource(DesktopCapturer::Source src)
        : source(src) {}
        DesktopCapturer::Source source;
        // source id
        DesktopCapturer::SourceId id() const { return source.id; }
        // source name
        std::string name() const { return source.title; }
        // Returns the thumbnail of the source, jpeg format.
        std::vector<unsigned char> thumbnail() const { return thumbnail_; }

        void SaveCaptureResult(webrtc::DesktopCapturer::Result result,
                                        std::unique_ptr<webrtc::DesktopFrame> frame);
    private:
        std::vector<unsigned char> thumbnail_;
    };
 public:
  ObjCDesktopMediaList(DesktopType type, RTC_OBJC_TYPE(RTCDesktopMediaList)* objcMediaList);
  virtual ~ObjCDesktopMediaList();

  virtual int32_t UpdateSourceList();

  virtual int GetSourceCount() const;
  
  virtual MediaSource* GetSource(int index);

 protected:
  virtual void OnMessage(rtc::Message* msg) override;

 private:
    class CallbackProxy : public DesktopCapturer::Callback {
        public:
         CallbackProxy(){}
          void SetCallback(std::function<void(webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame)> on_capture_result) {
                                on_capture_result_ = on_capture_result;
                               }
        private:
         void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame) override {
                                    if(on_capture_result_) on_capture_result_(result, std::move(frame));
                               }
        std::function<void(webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame)> on_capture_result_ = nullptr;
    };
 private:
  std::unique_ptr<CallbackProxy> callback_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  std::unique_ptr<rtc::Thread> thread_;
  std::vector<MediaSource*> sources_;
  RTC_OBJC_TYPE(RTCDesktopMediaList)* objcMediaList_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_MEDIA_LIST_H_
