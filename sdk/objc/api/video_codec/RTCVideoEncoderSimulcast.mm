#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoEncoderSimulcast.h"
#import "RTCWrappedNativeVideoEncoder.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"

#include "native/api/video_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderSimulcast)

+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)simulcastEncoderWithPrimary:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)primary
                                                         fallback:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)fallback
                                                   videoCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)videoCodecInfo {
    auto nativePrimary = webrtc::ObjCToNativeVideoEncoderFactory(primary);
    auto nativeFallback = webrtc::ObjCToNativeVideoEncoderFactory(fallback);
    auto nativeFormat = [videoCodecInfo nativeSdpVideoFormat];
    return [[RTC_OBJC_TYPE(RTCWrappedNativeVideoEncoder) alloc]
        initWithNativeEncoder: std::make_unique<webrtc::SimulcastEncoderAdapter>(
            nativePrimary.release(),
            nativeFallback.release(),
            std::move(nativeFormat))];
}

@end
