#import "RTCDesktopCapturer.h"

#include "sdk/objc/native/src/objc_desktop_capture.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCDesktopCapturer ()

@property(nonatomic, readonly)std::shared_ptr<webrtc::ObjCDesktopCapturer> nativeCapturer;

@end

NS_ASSUME_NONNULL_END