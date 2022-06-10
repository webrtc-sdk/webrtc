#import "RTCDesktopSource.h"

#include "sdk/objc/native/src/objc_desktop_media_list.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCDesktopSource ()

- (instancetype)initWithNativeSource:(const webrtc::ObjCDesktopMediaList::MediaSource*) nativeSource 
                          sourceType:(RTCDesktopSourceType) sourceType;

@property(nonatomic, readonly)const webrtc::ObjCDesktopMediaList::MediaSource* nativeMediaSource;

@end

NS_ASSUME_NONNULL_END