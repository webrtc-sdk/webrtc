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
#include "sdk/objc/native/src/objc_video_frame.h"
#include "rtc_base/checks.h"
#include "third_party/libyuv/include/libyuv.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
// Include directory supplied by gn
#include "jpeglib.h"  // NOLINT
#endif
}

#include <iostream>
#include <fstream>

namespace webrtc {

ObjCDesktopMediaList::ObjCDesktopMediaList(DesktopType type,
                                      RTC_OBJC_TYPE(RTCDesktopMediaList)* objcMediaList)
    :thread_(rtc::Thread::Create()),objcMediaList_(objcMediaList),type_(type) {
  options_ = webrtc::DesktopCaptureOptions::CreateDefault();
  options_.set_detect_updated_region(true);
  options_.set_allow_iosurface(true);
  if (type == kScreen) {
    capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options_);
  } else { 
    capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options_); 
  }
  callback_ = std::make_unique<CallbackProxy>();
  thread_->Start();
  capturer_->Start(callback_.get());
}

ObjCDesktopMediaList::~ObjCDesktopMediaList() {
  thread_->Stop();
}

int32_t ObjCDesktopMediaList::UpdateSourceList() {
    
  webrtc::DesktopCapturer::SourceList new_sources;
  capturer_->GetSourceList(&new_sources);

  typedef std::set<DesktopCapturer::SourceId> SourceSet;
  SourceSet new_source_set;
  for (size_t i = 0; i < new_sources.size(); ++i) {
    new_source_set.insert(new_sources[i].id);
  }
  // Iterate through the old sources to find the removed sources.
  for (size_t i = 0; i < sources_.size(); ++i) {
    if (new_source_set.find(sources_[i]->id()) == new_source_set.end()) {
      sources_.erase(sources_.begin() + i);
      [objcMediaList_ mediaSourceRemoved:i];
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
        MediaSource* source = new MediaSource(new_sources[i],type_);
        sources_.insert(sources_.begin() + i, std::shared_ptr<MediaSource>(source));
        [objcMediaList_ mediaSourceAdded:i];
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
        if (sources_[old_pos]->id() == new_sources[pos].id)
          break;
      }
      RTC_DCHECK(sources_[old_pos]->id() == new_sources[pos].id);

      // Move the source from |old_pos| to |pos|.
      auto temp = sources_[old_pos];
      sources_.erase(sources_.begin() + old_pos);
      sources_.insert(sources_.begin() + pos, temp);
      [objcMediaList_ mediaSourceMoved:old_pos newIndex:pos];
    }

    if (sources_[pos]->source.title != new_sources[pos].title) {
      sources_[pos]->source.title = new_sources[pos].title;
      [objcMediaList_ mediaSourceNameChanged:pos];
    }
    ++pos;
  }

  for( auto source : sources_) {
    callback_->SetCallback([&](webrtc::DesktopCapturer::Result result,
                               std::unique_ptr<webrtc::DesktopFrame> frame){
      source->SaveCaptureResult(result, std::move(frame));
    });
    if(capturer_->SelectSource(source->id())){
      capturer_->CaptureFrame();
    }
  }

  return sources_.size();
}


int ObjCDesktopMediaList::GetSourceCount() const {
    return sources_.size();
}
  
ObjCDesktopMediaList::MediaSource *ObjCDesktopMediaList::GetSource(int index) {
    return sources_[index].get();
}


ObjCDesktopMediaList::MediaSource::~MediaSource() {

}

void ObjCDesktopMediaList::MediaSource::SaveCaptureResult(webrtc::DesktopCapturer::Result result,
                                        std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }
  
  int quality = 80;
  const int kColorPlanes = 4;  // alpha, R, G and B.
  unsigned char* out_buffer = NULL;
  unsigned long out_size = 0;
  // Invoking LIBJPEG
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  jpeg_mem_dest(&cinfo, &out_buffer, &out_size);

  int width = frame->size().width();
  int height = frame->size().height();
  int real_width = width;

  if(type_ == kWindow) {
    int multiple = 0;
#if defined(WEBRTC_ARCH_X86_FAMILY)
    multiple = 16;
#elif defined(WEBRTC_ARCH_ARM64)
    multiple = 32;
#endif
    // A multiple of $multiple must be used as the width of the src frame,
    // and the right black border needs to be cropped during conversion.
    if( multiple != 0 && (width % multiple) != 0 ) {
      width = (width / multiple + 1) * multiple;
    }
  }

  cinfo.image_width = real_width;
  cinfo.image_height = height;
  cinfo.input_components = kColorPlanes;
  cinfo.in_color_space = JCS_EXT_BGRA;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  int row_stride = width * kColorPlanes;
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &frame->data()[cinfo.next_scanline * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  thumbnail_.resize(out_size);

  std::copy(out_buffer
        , out_buffer + out_size
        , thumbnail_.begin());

  free(out_buffer);
}

void ObjCDesktopMediaList::OnMessage(rtc::Message* msg) {
  
}
}  // namespace webrtc
