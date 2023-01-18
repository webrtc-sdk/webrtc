/*
 * Copyright 2023 LiveKit
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

#import "RTCRtpCodecCapability+Private.h"

#import "RTCRtpReceiver+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCRtpCodecCapability)

@synthesize nativeCodecCapability = _nativeCodecCapability;

- (instancetype)init {
  return [self initWithNativeCodecCapability:webrtc::RtpCodecCapability()];
}

- (instancetype)initWithNativeCodecCapability:
    (const webrtc::RtpCodecCapability &)nativeCodecCapability {
  if (self = [super init]) {
    _nativeCodecCapability = nativeCodecCapability;
  }

  return self;
}

- (NSString *)mimeType {
  return [NSString stringWithUTF8String:_nativeCodecCapability.mime_type().c_str()];
}

- (NSString *)name {
  return [NSString stringWithUTF8String:_nativeCodecCapability.name.c_str()];
}

- (void)setName:(NSString *)name {
  _nativeCodecCapability.name = std::string([name UTF8String]);
}

- (RTCRtpMediaType)kind {
  return [RTCRtpReceiver mediaTypeForNativeMediaType:_nativeCodecCapability.kind];
}

- (void)setKind:(RTCRtpMediaType)kind {
  _nativeCodecCapability.kind = [RTCRtpReceiver nativeMediaTypeForMediaType:kind];
}

- (NSNumber *)clockRate {
  if (!_nativeCodecCapability.clock_rate) {
    return nil;
  }

  return [NSNumber numberWithInt:*_nativeCodecCapability.clock_rate];
}

- (void)setClockRate:(NSNumber *)clockRate {
  if (clockRate == nil) {
    _nativeCodecCapability.clock_rate = absl::optional<int>();
    return;
  }

  _nativeCodecCapability.clock_rate = absl::optional<int>(clockRate.intValue);
}

- (NSNumber *)preferredPayloadType {
  if (!_nativeCodecCapability.preferred_payload_type) {
    return nil;
  }

  return [NSNumber numberWithInt:*_nativeCodecCapability.preferred_payload_type];
}

- (void)setPreferredPayloadType:(NSNumber *)preferredPayloadType {
  if (preferredPayloadType == nil) {
    _nativeCodecCapability.preferred_payload_type = absl::optional<int>();
    return;
  }

  _nativeCodecCapability.preferred_payload_type =
      absl::optional<int>(preferredPayloadType.intValue);
}

- (NSNumber *)numChannels {
  if (!_nativeCodecCapability.num_channels) {
    return nil;
  }

  return [NSNumber numberWithInt:*_nativeCodecCapability.num_channels];
}

- (void)setNumChannels:(NSNumber *)numChannels {
  if (numChannels == nil) {
    _nativeCodecCapability.num_channels = absl::optional<int>();
    return;
  }

  _nativeCodecCapability.num_channels = absl::optional<int>(numChannels.intValue);
}

- (NSDictionary<NSString *, NSString *> *)parameters {
  NSMutableDictionary *result = [NSMutableDictionary dictionary];
  auto _parameters = _nativeCodecCapability.parameters;
  for (auto it = _parameters.begin(); it != _parameters.end(); ++it) {
    [result setObject:[NSString stringForStdString:it->second]
               forKey:[NSString stringForStdString:it->first]];
  }

  return result;
}

- (void)setParameters:(NSDictionary<NSString *, NSString *> *)parameters {
  std::map<std::string, std::string> _parameters;
  for (NSString *paramKey in parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:parameters[paramKey]];
    _parameters[key] = value;
  }

  _nativeCodecCapability.parameters = _parameters;
}

@end
