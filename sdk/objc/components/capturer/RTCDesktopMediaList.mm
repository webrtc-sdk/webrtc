#import "RTCDesktopMediaList.h"

#include "sdk/objc/native/src/objc_desktop_media_list.h"

@implementation RTCDesktopMediaList {
     std::unique_ptr<webrtc::ObjCDesktopMediaList> media_list_;
     RTCDesktopCapturerSourceType _sourceType;
     NSArray<RTCDesktopCapturerSource *>* _sources;
}

@synthesize sourceType = _sourceType;
@synthesize sources  = _sources;

- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)>)delegate type:(RTCDesktopCapturerSourceType)type {
    if (self = [super init]) {
        webrtc::ObjCDesktopMediaList::DesktopType captureType = webrtc::ObjCDesktopMediaList::kScreen;
        if(type == RTCDesktopCapturerSourceTypeWindow) {
            captureType = webrtc::ObjCDesktopMediaList::kWindow;
        }
        media_list_ = std::make_unique<webrtc::ObjCDesktopMediaList>(captureType, self);
    }
    return self;
}

- (void)startUpdating {
    media_list_->UpdateSourceList();
}

- (void)stopUpdating {

}

@end