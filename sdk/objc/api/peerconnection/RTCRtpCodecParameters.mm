/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecParameters+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

const NSString * const RTC_CONSTANT_TYPE(RTCRtxCodecName) = @(webrtc::kRtxCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCRedCodecName) = @(webrtc::kRedCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCUlpfecCodecName) = @(webrtc::kUlpfecCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCFlexfecCodecName) = @(webrtc::kFlexfecCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCOpusCodecName) = @(webrtc::kOpusCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCL16CodecName)  = @(webrtc::kL16CodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCG722CodecName) = @(webrtc::kG722CodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCPcmuCodecName) = @(webrtc::kPcmuCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCPcmaCodecName) = @(webrtc::kPcmaCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCDtmfCodecName) = @(webrtc::kDtmfCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCComfortNoiseCodecName) =
    @(webrtc::kComfortNoiseCodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCVp8CodecName) = @(webrtc::kVp8CodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCVp9CodecName) = @(webrtc::kVp9CodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCH264CodecName) = @(webrtc::kH264CodecName);
const NSString * const RTC_CONSTANT_TYPE(RTCAv1CodecName) = @(webrtc::kAv1CodecName);

@implementation RTC_OBJC_TYPE (RTCRtpCodecParameters)

@synthesize payloadType = _payloadType;
@synthesize name = _name;
@synthesize kind = _kind;
@synthesize clockRate = _clockRate;
@synthesize numChannels = _numChannels;
@synthesize parameters = _parameters;

- (instancetype)init {
  webrtc::RtpCodecParameters nativeParameters;
  return [self initWithNativeParameters:nativeParameters];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters {
  self = [super init];
  if (self) {
    _payloadType = nativeParameters.payload_type;
    _name = [NSString stringForStdString:nativeParameters.name];
    switch (nativeParameters.kind) {
      case webrtc::MediaType::AUDIO:
        _kind = RTC_CONSTANT_TYPE(RTCMediaStreamTrackKindAudio);
        break;
      case webrtc::MediaType::VIDEO:
        _kind = RTC_CONSTANT_TYPE(RTCMediaStreamTrackKindVideo);
        break;
      default:
        RTC_DCHECK_NOTREACHED();
        break;
    }
    if (nativeParameters.clock_rate) {
      _clockRate = [NSNumber numberWithInt:*nativeParameters.clock_rate];
    }
    if (nativeParameters.num_channels) {
      _numChannels = [NSNumber numberWithInt:*nativeParameters.num_channels];
    }
    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];
    for (const auto &parameter : nativeParameters.parameters) {
      [parameters setObject:[NSString stringForStdString:parameter.second]
                     forKey:[NSString stringForStdString:parameter.first]];
    }
    _parameters = parameters;
  }
  return self;
}

- (webrtc::RtpCodecParameters)nativeParameters {
  webrtc::RtpCodecParameters parameters;
  parameters.payload_type = _payloadType;
  parameters.name = [NSString stdStringForString:_name];
  // NSString pointer comparison is safe here since "kind" is readonly and only
  // populated above.
  if (_kind == RTC_CONSTANT_TYPE(RTCMediaStreamTrackKindAudio)) {
    parameters.kind = webrtc::MediaType::AUDIO;
  } else if (_kind == RTC_CONSTANT_TYPE(RTCMediaStreamTrackKindVideo)) {
    parameters.kind = webrtc::MediaType::VIDEO;
  } else {
    RTC_DCHECK_NOTREACHED();
  }
  if (_clockRate != nil) {
    parameters.clock_rate = std::optional<int>(_clockRate.intValue);
  }
  if (_numChannels != nil) {
    parameters.num_channels = std::optional<int>(_numChannels.intValue);
  }
  for (NSString *paramKey in _parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:_parameters[paramKey]];
    parameters.parameters[key] = value;
  }
  return parameters;
}

@end
