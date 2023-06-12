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

#include "sdk/objc/native/src/objc_desktop_media_list.h"
#include "rtc_base/checks.h"
#include "sdk/objc/native/src/objc_video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
// Include directory supplied by gn
#include "jpeglib.h"  // NOLINT
#endif
}

#include <fstream>
#include <iostream>

#import <CoreImage/CoreImage.h>

namespace webrtc {

ObjCDesktopMediaList::ObjCDesktopMediaList(DesktopType type,
                                           RTC_OBJC_TYPE(RTCDesktopMediaList) * objcMediaList)
    : thread_(rtc::Thread::Create()), objcMediaList_(objcMediaList), type_(type) {
  RTC_DCHECK(thread_);
  thread_->Start();
  options_ = webrtc::DesktopCaptureOptions::CreateDefault();
  options_.set_detect_updated_region(true);
  options_.set_allow_iosurface(true);

  callback_ = std::make_unique<CallbackProxy>();

  thread_->BlockingCall([this, type] {
     if (type == kScreen) {
      capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options_);
    } else {
      capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options_);
    }
    capturer_->Start(callback_.get());
  });
}

ObjCDesktopMediaList::~ObjCDesktopMediaList() {
  thread_->BlockingCall([this] {
    capturer_.reset();
  });
}

int32_t ObjCDesktopMediaList::UpdateSourceList(bool force_reload, bool get_thumbnail) {
  if (force_reload) {
    for (auto source : sources_) {
      [objcMediaList_ mediaSourceRemoved:source.get()];
    }
    sources_.clear();
  }

  webrtc::DesktopCapturer::SourceList new_sources;

  thread_->BlockingCall([this, &new_sources] {
    capturer_->GetSourceList(&new_sources);
  });

  typedef std::set<DesktopCapturer::SourceId> SourceSet;
  SourceSet new_source_set;
  for (size_t i = 0; i < new_sources.size(); ++i) {
    if (type_ == kScreen && new_sources[i].title.length() == 0) {
      new_sources[i].title = std::string("Screen " + std::to_string(i + 1));
    }
    new_source_set.insert(new_sources[i].id);
  }
  // Iterate through the old sources to find the removed sources.
  for (size_t i = 0; i < sources_.size(); ++i) {
    if (new_source_set.find(sources_[i]->id()) == new_source_set.end()) {
      [objcMediaList_ mediaSourceRemoved:(*(sources_.begin() + i)).get()];
      sources_.erase(sources_.begin() + i);
      --i;
    }
  }
  // Iterate through the new sources to find the added sources.
  if (new_sources.size() > sources_.size()) {
    SourceSet old_source_set;
    for (size_t i = 0; i < sources_.size(); ++i) {
      old_source_set.insert(sources_[i]->id());
    }
    for (size_t i = 0; i < new_sources.size(); ++i) {
      if (old_source_set.find(new_sources[i].id) == old_source_set.end()) {
        MediaSource *source = new MediaSource(this, new_sources[i], type_);
        sources_.insert(sources_.begin() + i, std::shared_ptr<MediaSource>(source));
        [objcMediaList_ mediaSourceAdded:source];
        GetThumbnail(source, true);
      }
    }
  }

  RTC_DCHECK_EQ(new_sources.size(), sources_.size());

  // Find the moved/changed sources.
  size_t pos = 0;
  while (pos < sources_.size()) {
    if (!(sources_[pos]->id() == new_sources[pos].id)) {
      // Find the source that should be moved to |pos|, starting from |pos + 1|
      // of |sources_|, because entries before |pos| should have been sorted.
      size_t old_pos = pos + 1;
      for (; old_pos < sources_.size(); ++old_pos) {
        if (sources_[old_pos]->id() == new_sources[pos].id) break;
      }
      RTC_DCHECK(sources_[old_pos]->id() == new_sources[pos].id);

      // Move the source from |old_pos| to |pos|.
      auto temp = sources_[old_pos];
      sources_.erase(sources_.begin() + old_pos);
      sources_.insert(sources_.begin() + pos, temp);
      //[objcMediaList_ mediaSourceMoved:old_pos newIndex:pos];
    }

    if (sources_[pos]->source.title != new_sources[pos].title) {
      sources_[pos]->source.title = new_sources[pos].title;
      [objcMediaList_ mediaSourceNameChanged:sources_[pos].get()];
    }
    ++pos;
  }

  if (get_thumbnail) {
    for (auto source : sources_) {
      GetThumbnail(source.get(), true);
    }
  }
  return sources_.size();
}

bool ObjCDesktopMediaList::GetThumbnail(MediaSource *source, bool notify) {
  thread_->PostTask([this, source, notify] {
      if(capturer_->SelectSource(source->id())){
        callback_->SetCallback([&](webrtc::DesktopCapturer::Result result,
                             std::unique_ptr<webrtc::DesktopFrame> frame) {
          auto old_thumbnail = source->thumbnail();
          source->SaveCaptureResult(result, std::move(frame));
          if(old_thumbnail.size() != source->thumbnail().size() && notify) {
            [objcMediaList_ mediaSourceThumbnailChanged:source];
          }
        });
        capturer_->CaptureFrame();
      }
  });

  return true;
}

int ObjCDesktopMediaList::GetSourceCount() const {
  return sources_.size();
}

MediaSource *ObjCDesktopMediaList::GetSource(int index) {
  return sources_[index].get();
}

bool MediaSource::UpdateThumbnail() {
  return mediaList_->GetThumbnail(this, true);
}

void MediaSource::SaveCaptureResult(webrtc::DesktopCapturer::Result result,
                                    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }
  int width = frame->size().width();
  int height = frame->size().height();
  int real_width = width;

  if (type_ == kWindow) {
    int multiple = 0;
#if defined(WEBRTC_ARCH_X86_FAMILY)
    multiple = 16;
#elif defined(WEBRTC_ARCH_ARM64)
    multiple = 32;
#endif
    // A multiple of $multiple must be used as the width of the src frame,
    // and the right black border needs to be cropped during conversion.
    if (multiple != 0 && (width % multiple) != 0) {
      width = (width / multiple + 1) * multiple;
    }
  }

  CVPixelBufferRef pixelBuffer = NULL;

  NSDictionary *pixelAttributes = @{(NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{}};
  CVReturn res = CVPixelBufferCreate(kCFAllocatorDefault,
                                     width,
                                     height,
                                     kCVPixelFormatType_32BGRA,
                                     (__bridge CFDictionaryRef)(pixelAttributes),
                                     &pixelBuffer);
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  uint8_t *pxdata = (uint8_t *)CVPixelBufferGetBaseAddress(pixelBuffer);
  libyuv::ConvertToARGB(reinterpret_cast<uint8_t *>(frame->data()),
                        real_width * height * 4,
                        reinterpret_cast<uint8_t *>(pxdata),
                        width * 4,
                        0,
                        0,
                        width,
                        height,
                        real_width,
                        height,
                        libyuv::kRotate0,
                        libyuv::FOURCC_ARGB);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  if (res != kCVReturnSuccess) {
    NSLog(@"Unable to create cvpixelbuffer %d", res);
    return;
  }

  CIImage *ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
  CGRect outputSize = CGRectMake(0, 0, width, height);

  CIContext *tempContext = [CIContext contextWithOptions:nil];
  CGImageRef cgImage = [tempContext createCGImage:ciImage fromRect:outputSize];
  NSData *imageData;
  NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
  [newRep setSize:NSSizeToCGSize(outputSize.size)];
  imageData = [newRep representationUsingType:NSJPEGFileType
                                   properties:@{
                                     NSImageCompressionFactor : @1.0f
                                   }];

  thumbnail_.resize(imageData.length);
  const void *_Nullable rawData = [imageData bytes];
  char *src = (char *)rawData;
  std::copy(src, src + imageData.length, thumbnail_.begin());

  CGImageRelease(cgImage);
  CVPixelBufferRelease(pixelBuffer);
}

}  // namespace webrtc
