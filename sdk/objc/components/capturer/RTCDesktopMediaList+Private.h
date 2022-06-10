#import "RTCDesktopMediaList.h"

#include "sdk/objc/native/src/objc_desktop_media_list.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCDesktopMediaList ()

@property(nonatomic, readonly)std::shared_ptr<webrtc::ObjCDesktopMediaList> nativeMediaList;

@end

NS_ASSUME_NONNULL_END