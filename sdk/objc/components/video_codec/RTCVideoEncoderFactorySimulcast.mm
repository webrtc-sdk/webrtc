#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCodecInfo.h"
#import "RTCVideoEncoderFactorySimulcast.h"
#import "api/video_codec/RTCVideoEncoderSimulcast.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"

#include "absl/container/inlined_vector.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "media/base/media_constants.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderFactorySimulcast) ()

@property id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)> primary;
@property id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)> fallback;

@end


@implementation RTC_OBJC_TYPE (RTCVideoEncoderFactorySimulcast)

@synthesize primary = _primary;
@synthesize fallback = _fallback;

- (instancetype)initWithPrimary:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)primary
                       fallback:(id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)fallback {
    if (self = [super init]) {
        _primary = primary;
        _fallback = fallback;
    }
    return self;
}

- (nullable id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder: (RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
    return [RTCVideoEncoderSimulcast simulcastEncoderWithPrimary: _primary fallback: _fallback videoCodecInfo: info];
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
    NSArray *supportedCodecs = [[_primary supportedCodecs] arrayByAddingObjectsFromArray: [_fallback supportedCodecs]];

    NSMutableArray<RTCVideoCodecInfo *> *addingCodecs = [[NSMutableArray alloc] init];

    for (const webrtc::SdpVideoFormat& format : webrtc::SupportedVP9Codecs(true)) {
        RTCVideoCodecInfo *codec = [[RTCVideoCodecInfo alloc] initWithNativeSdpVideoFormat: format];
        [addingCodecs addObject: codec];
    }

    auto av1Format = webrtc::SdpVideoFormat(
         cricket::kAv1CodecName, webrtc::CodecParameterMap(),
         webrtc::LibaomAv1EncoderSupportedScalabilityModes());
    RTCVideoCodecInfo *av1Codec = [[RTCVideoCodecInfo alloc] initWithNativeSdpVideoFormat: av1Format];
    [addingCodecs addObject: av1Codec];

    return [supportedCodecs arrayByAddingObjectsFromArray: addingCodecs];
}


@end
