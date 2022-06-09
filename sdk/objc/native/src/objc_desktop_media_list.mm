#include "sdk/objc/native/src/objc_desktop_media_list.h"
#include "sdk/objc/native/src/objc_video_frame.h"
#include "rtc_base/checks.h"
#include "third_party/libyuv/include/libyuv.h"

#import "components/capturer/RTCDesktopMediaList.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
// Include directory supplied by gn
#include "jpeglib.h"  // NOLINT
#endif
}

namespace webrtc {

ObjCDesktopMediaList::ObjCDesktopMediaList(DesktopType type,
                                     id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)> delegate)
    : thread_(rtc::Thread::Create()), delegate_(delegate) {
  webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();
  if (type == kScreen) {
    capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
  } else { 
      capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options); 
  }
  thread_->Start();
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
    if (new_source_set.find(sources_[i].id()) == new_source_set.end()) {
      sources_.erase(sources_.begin() + i);
      [delegate_ mediaSourceRemoved:i];
      --i;
    }
  }
  // Iterate through the new sources to find the added sources.
  if (new_sources.size() > sources_.size()) {
    SourceSet old_source_set;
    for (size_t i = 0; i < sources_.size(); ++i) {
      old_source_set.insert(sources_[i].id());
    }

    for (size_t i = 0; i < new_sources.size(); ++i) {
      if (old_source_set.find(new_sources[i].id) == old_source_set.end()) {
        sources_.insert(sources_.begin() + i, MediaSource(new_sources[i], capturer_.get(),[&](DesktopCapturer::SourceId sourceId) {
            for (size_t i = 0; i < sources_.size(); ++i) {
                if (sources_[i].id() == sourceId) {
                [delegate_ mediaSourceThumbnailChanged:i];
                break;
                }
            }
        }));
        [delegate_ mediaSourceAdded:i];
      }
    }
  }

  RTC_DCHECK_EQ(new_sources.size(), sources_.size());

  // Find the moved/changed sources.
  size_t pos = 0;
  while (pos < sources_.size()) {
    if (!(sources_[pos].id() == new_sources[pos].id)) {
      // Find the source that should be moved to |pos|, starting from |pos + 1|
      // of |sources_|, because entries before |pos| should have been sorted.
      size_t old_pos = pos + 1;
      for (; old_pos < sources_.size(); ++old_pos) {
        if (sources_[old_pos].id() == new_sources[pos].id)
          break;
      }
      RTC_DCHECK(sources_[old_pos].id() == new_sources[pos].id);

      // Move the source from |old_pos| to |pos|.
      MediaSource temp = sources_[old_pos];
      sources_.erase(sources_.begin() + old_pos);
      sources_.insert(sources_.begin() + pos, temp);
      [delegate_ mediaSourceMoved:old_pos newIndex:pos];
    }

    if (sources_[pos].source.title != new_sources[pos].title) {
      sources_[pos].source.title = new_sources[pos].title;
      [delegate_ mediaSourceNameChanged:pos];
    }
    ++pos;
  }

  return sources_.size();
}


int ObjCDesktopMediaList::GetSourceCount() const {
    return sources_.size();
}
  
const ObjCDesktopMediaList::MediaSource& ObjCDesktopMediaList::GetSource(int index) const {
    return sources_[index];
}

void ObjCDesktopMediaList::MediaSource::UpdateThumbnail(int width, int height) {
    if(capturer_->SelectSource(source.id) && capturer_->FocusOnSelectedSource()) {
        capturer_->Start(this);
        capturer_->CaptureFrame();
    }
}
void ObjCDesktopMediaList::MediaSource::OnCaptureResult(webrtc::DesktopCapturer::Result result,
                                        std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }

  int quality = 80;
  const int kColorPlanes = 4;  // alpha, R, G and B.
  unsigned char* out_buffer;
  unsigned long out_size;
  // Invoking LIBJPEG
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  jpeg_mem_dest(&cinfo, &out_buffer, &out_size);

  cinfo.image_width = frame->size().width();
  cinfo.image_height = frame->size().height();
  cinfo.input_components = kColorPlanes;
  cinfo.in_color_space = JCS_EXT_ARGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  int row_stride = frame->size().width() * kColorPlanes;
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
  on_thumbnail_update_(id());
}

void ObjCDesktopMediaList::OnMessage(rtc::Message* msg) {
  
}
}  // namespace webrtc
