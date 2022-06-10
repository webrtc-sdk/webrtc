/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
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

@protocol RTC_OBJC_TYPE
(RTCDesktopMediaListDelegate);

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

    class MediaSource : public DesktopCapturer::Callback {
    public:
        MediaSource(DesktopCapturer::Source src,  webrtc::DesktopCapturer* capturer, 
                    std::function<void(DesktopCapturer::SourceId)> on_thumbnail_update)
        : source(src),capturer_(capturer),on_thumbnail_update_(on_thumbnail_update) {}
        DesktopCapturer::Source source;
        // source id
        DesktopCapturer::SourceId id() const { return source.id; }
        // source name
        std::string name() const { return source.title; }
        // Returns the thumbnail of the source, jpeg format.
        std::vector<unsigned char> thumbnail() const { return thumbnail_; }
        // Get the thumbnail of the source.
        void UpdateThumbnail(int width, int height);
    private:
        void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame) override;
        std::vector<unsigned char> thumbnail_;
        webrtc::DesktopCapturer* capturer_;
        std::function<void(DesktopCapturer::SourceId)> on_thumbnail_update_;
    };
 public:
  ObjCDesktopMediaList(DesktopType type, id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)> delegate);
  virtual ~ObjCDesktopMediaList();

  virtual int32_t UpdateSourceList();

  virtual int GetSourceCount() const;
  
  virtual const MediaSource& GetSource(int index) const;

 protected:
  virtual void OnMessage(rtc::Message* msg) override;

 private:
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  std::unique_ptr<rtc::Thread> thread_;
  std::vector<MediaSource> sources_;
  id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)> delegate_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_DESKTOP_MEDIA_LIST_H_
