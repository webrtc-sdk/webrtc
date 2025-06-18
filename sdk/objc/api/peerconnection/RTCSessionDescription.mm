/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCSessionDescription+Private.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/jsep.h"
#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCSessionDescription)

@synthesize type = _type;
@synthesize sdp = _sdp;

+ (NSString *)stringForType:(RTC_OBJC_TYPE(RTCSdpType))type {
  std::string string = [[self class] stdStringForType:type];
  return [NSString stringForStdString:string];
}

+ (RTC_OBJC_TYPE(RTCSdpType))typeForString:(NSString *)string {
  std::string typeString = string.stdString;
  return [[self class] typeForStdString:typeString];
}

- (instancetype)initWithType:(RTC_OBJC_TYPE(RTCSdpType))type sdp:(NSString *)sdp {
  self = [super init];
  if (self) {
    _type = type;
    _sdp = [sdp copy];
  }
  return self;
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"RTC_OBJC_TYPE(RTCSessionDescription):\n%@\n%@",
                       [[self class] stringForType:_type],
                       _sdp];
}

#pragma mark - Private

- (std::unique_ptr<webrtc::SessionDescriptionInterface>)nativeDescription {
  webrtc::SdpParseError error;

  std::unique_ptr<webrtc::SessionDescriptionInterface> description(
      webrtc::CreateSessionDescription(
          [[self class] nativeTypeForType:_type], _sdp.stdString, &error));

  if (!description) {
    RTCLogError(@"Failed to create session description: %s\nline: %s",
                error.description.c_str(),
                error.line.c_str());
  }

  return description;
}

- (instancetype)initWithNativeDescription:
    (const webrtc::SessionDescriptionInterface *)nativeDescription {
  NSParameterAssert(nativeDescription);
  std::string sdp;
  nativeDescription->ToString(&sdp);
  RTC_OBJC_TYPE(RTCSdpType) type = [[self class] typeForStdString:nativeDescription->type()];

  return [self initWithType:type sdp:[NSString stringForStdString:sdp]];
}

+ (std::string)stdStringForType:(RTC_OBJC_TYPE(RTCSdpType))type {
  switch (type) {
    case RTC_OBJC_TYPE(RTCSdpTypeOffer):
      return webrtc::SessionDescriptionInterface::kOffer;
    case RTC_OBJC_TYPE(RTCSdpTypePrAnswer):
      return webrtc::SessionDescriptionInterface::kPrAnswer;
    case RTC_OBJC_TYPE(RTCSdpTypeAnswer):
      return webrtc::SessionDescriptionInterface::kAnswer;
    case RTC_OBJC_TYPE(RTCSdpTypeRollback):
      return webrtc::SessionDescriptionInterface::kRollback;
  }
}

+ (RTC_OBJC_TYPE(RTCSdpType))typeForStdString:(const std::string &)string {
  if (string == webrtc::SessionDescriptionInterface::kOffer) {
    return RTC_OBJC_TYPE(RTCSdpTypeOffer);
  } else if (string == webrtc::SessionDescriptionInterface::kPrAnswer) {
    return RTC_OBJC_TYPE(RTCSdpTypePrAnswer);
  } else if (string == webrtc::SessionDescriptionInterface::kAnswer) {
    return RTC_OBJC_TYPE(RTCSdpTypeAnswer);
  } else if (string == webrtc::SessionDescriptionInterface::kRollback) {
    return RTC_OBJC_TYPE(RTCSdpTypeRollback);
  } else {
    RTC_DCHECK_NOTREACHED();
    return RTC_OBJC_TYPE(RTCSdpTypeOffer);
  }
}

+ (webrtc::SdpType)nativeTypeForType:(RTC_OBJC_TYPE(RTCSdpType))type {
  switch (type) {
    case RTC_OBJC_TYPE(RTCSdpTypeOffer):
      return webrtc::SdpType::kOffer;
    case RTC_OBJC_TYPE(RTCSdpTypePrAnswer):
      return webrtc::SdpType::kPrAnswer;
    case RTC_OBJC_TYPE(RTCSdpTypeAnswer):
      return webrtc::SdpType::kAnswer;
    case RTC_OBJC_TYPE(RTCSdpTypeRollback):
      return webrtc::SdpType::kRollback;
  }
}

@end
