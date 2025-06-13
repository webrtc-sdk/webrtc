#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoEncoderSimulcast.h"
#import "RTCNativeVideoEncoder.h"
#import "RTCNativeVideoEncoderBuilder+Native.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"
#include "api/transport/field_trial_based_config.h"

#include "native/api/video_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderSimulcastBuilder)
    : RTC_OBJC_TYPE(RTCNativeVideoEncoder) <RTC_OBJC_TYPE (RTCNativeVideoEncoderBuilder)> {

    id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)> _primary;
    id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)> _fallback;
    RTC_OBJC_TYPE(RTCVideoCodecInfo) *_videoCodecInfo;
}

- (id)initWithPrimary:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)primary
             fallback:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)fallback
       videoCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)videoCodecInfo;

@end

@implementation RTC_OBJC_TYPE (RTCVideoEncoderSimulcastBuilder)

- (std::unique_ptr<webrtc::VideoEncoder>)build:(const webrtc::Environment&)env {
    auto nativePrimary = webrtc::ObjCToNativeVideoEncoderFactory(_primary);
    auto nativeFallback = webrtc::ObjCToNativeVideoEncoderFactory(_fallback);
    auto nativeFormat = [_videoCodecInfo nativeSdpVideoFormat];
    return std::make_unique<webrtc::SimulcastEncoderAdapter>(
            env,
            nativePrimary.release(),
            nativeFallback.release(),
            std::move(nativeFormat));
}

- (id)initWithPrimary:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)primary
             fallback:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)fallback
       videoCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)videoCodecInfo {
    if (self = [super init]) {
        self->_primary = primary;
        self->_fallback = fallback;
        self->_videoCodecInfo = videoCodecInfo;
    }
    return self;
}

@end

@implementation RTC_OBJC_TYPE (RTCVideoEncoderSimulcast)

+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)simulcastEncoderWithPrimary:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)primary
                                                         fallback:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)fallback
                                                   videoCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)videoCodecInfo {
    return [[RTC_OBJC_TYPE(RTCVideoEncoderSimulcastBuilder) alloc]
        initWithPrimary:primary
               fallback:fallback
         videoCodecInfo:videoCodecInfo];
}

@end
