/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpTransceiver+Private.h"

#import "RTCRtpCodecCapability+Private.h"
#import "RTCRtpEncodingParameters+Private.h"
#import "RTCRtpHeaderExtensionCapability+Private.h"
#import "RTCRtpParameters+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"
#import "RTCRtpCodecCapability.h"
#import "RTCRtpCodecCapability+Private.h"
#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/rtp_parameters.h"

NSString *const RTC_CONSTANT_TYPE(RTCRtpTransceiverErrorDomain) = @"org.webrtc.RTCRtpTranceiver";

@implementation RTC_OBJC_TYPE (RTCRtpTransceiverInit)

@synthesize direction = _direction;
@synthesize streamIds = _streamIds;
@synthesize sendEncodings = _sendEncodings;

- (instancetype)init {
  self = [super init];
  if (self) {
    _direction = RTC_OBJC_TYPE(RTCRtpTransceiverDirectionSendRecv);
  }
  return self;
}

- (webrtc::RtpTransceiverInit)nativeInit {
  webrtc::RtpTransceiverInit init;
  init.direction = [RTC_OBJC_TYPE(RTCRtpTransceiver)
      nativeRtpTransceiverDirectionFromDirection:_direction];
  for (NSString *streamId in _streamIds) {
    init.stream_ids.push_back([streamId UTF8String]);
  }
  for (RTC_OBJC_TYPE(RTCRtpEncodingParameters) *
       sendEncoding in _sendEncodings) {
    init.send_encodings.push_back(sendEncoding.nativeParameters);
  }
  return init;
}

@end

@implementation RTC_OBJC_TYPE (RTCRtpTransceiver) {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) * _factory;
  webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> _nativeRtpTransceiver;
}

- (RTC_OBJC_TYPE(RTCRtpMediaType))mediaType {
  return [RTC_OBJC_TYPE(RTCRtpReceiver)
      mediaTypeForNativeMediaType:_nativeRtpTransceiver->media_type()];
}

- (NSString *)mid {
  if (_nativeRtpTransceiver->mid()) {
    return [NSString stringForStdString:*_nativeRtpTransceiver->mid()];
  } else {
    return nil;
  }
}

- (NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecPreferences {

  NSMutableArray *result = [NSMutableArray array];

  std::vector<webrtc::RtpCodecCapability> capabilities = _nativeRtpTransceiver->codec_preferences();

  for (auto & element : capabilities) {
    RTC_OBJC_TYPE(RTCRtpCodecCapability) *object = [[RTC_OBJC_TYPE(RTCRtpCodecCapability) alloc] initWithNativeRtpCodecCapability: element];
    [result addObject: object];
  }

  return result;
}

@synthesize sender = _sender;
@synthesize receiver = _receiver;

- (BOOL)isStopped {
  return _nativeRtpTransceiver->stopped();
}

- (RTC_OBJC_TYPE(RTCRtpTransceiverDirection))direction {
  return [RTC_OBJC_TYPE(RTCRtpTransceiver)
      rtpTransceiverDirectionFromNativeDirection:_nativeRtpTransceiver
                                                     ->direction()];
}

- (NSArray<RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) *> *)
    headerExtensionsToNegotiate {
  std::vector<webrtc::RtpHeaderExtensionCapability> nativeHeaderExtensions(
      _nativeRtpTransceiver->GetHeaderExtensionsToNegotiate());

  NSMutableArray *headerExtensions =
      [NSMutableArray arrayWithCapacity:nativeHeaderExtensions.size()];
  for (const auto &headerExtension : nativeHeaderExtensions) {
    [headerExtensions
        addObject:
            [[RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) alloc]
                initWithNativeRtpHeaderExtensionCapability:headerExtension]];
  }
  return headerExtensions;
}

- (NSArray<RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) *> *)
    negotiatedHeaderExtensions {
  std::vector<webrtc::RtpHeaderExtensionCapability> nativeHeaderExtensions(
      _nativeRtpTransceiver->GetNegotiatedHeaderExtensions());

  NSMutableArray *headerExtensions =
      [NSMutableArray arrayWithCapacity:nativeHeaderExtensions.size()];
  for (const auto &headerExtension : nativeHeaderExtensions) {
    [headerExtensions
        addObject:
            [[RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) alloc]
                initWithNativeRtpHeaderExtensionCapability:headerExtension]];
  }
  return headerExtensions;
}

- (void)setDirection:(RTC_OBJC_TYPE(RTCRtpTransceiverDirection))direction error:(NSError **)error {
  webrtc::RTCError nativeError = _nativeRtpTransceiver->SetDirectionWithError(
      [RTC_OBJC_TYPE(RTCRtpTransceiver)
          nativeRtpTransceiverDirectionFromDirection:direction]);

  if (!nativeError.ok() && error) {
    NSDictionary *userInfo = @{
      NSLocalizedDescriptionKey :
          [NSString stringWithCString:nativeError.message()
                             encoding:NSUTF8StringEncoding]
    };
    *error = [NSError errorWithDomain:RTC_CONSTANT_TYPE(RTCRtpTransceiverErrorDomain)
                                 code:static_cast<int>(nativeError.type())
                             userInfo:userInfo];
  }
}

- (BOOL)currentDirection:(RTC_OBJC_TYPE(RTCRtpTransceiverDirection) *)currentDirectionOut {
  if (_nativeRtpTransceiver->current_direction()) {
    *currentDirectionOut = [RTC_OBJC_TYPE(RTCRtpTransceiver)
        rtpTransceiverDirectionFromNativeDirection:*_nativeRtpTransceiver
                                                        ->current_direction()];
    return YES;
  } else {
    return NO;
  }
}

- (void)stopInternal {
  _nativeRtpTransceiver->StopInternal();
}

- (BOOL)setCodecPreferences:
            (NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecs
                      error:(NSError **)error {
  std::vector<webrtc::RtpCodecCapability> codecCapabilities;
  if (codecs) {
    for (RTC_OBJC_TYPE(RTCRtpCodecCapability) * rtpCodecCapability in codecs) {
      codecCapabilities.push_back(rtpCodecCapability.nativeRtpCodecCapability);
    }
  }
  webrtc::RTCError nativeError =
      _nativeRtpTransceiver->SetCodecPreferences(codecCapabilities);
  if (!nativeError.ok() && error) {
    *error = [NSError errorWithDomain:RTC_CONSTANT_TYPE(RTCRtpTransceiverErrorDomain)
                                 code:static_cast<int>(nativeError.type())
                             userInfo:@{
                               @"message" : [NSString
                                   stringWithUTF8String:nativeError.message()]
                             }];
  }
  return nativeError.ok();
}

- (void)setCodecPreferences:
    (NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecs {
  [self setCodecPreferences:codecs error:nil];
}

- (BOOL)setHeaderExtensionsToNegotiate:
            (NSArray<RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) *> *)
                extensions
                                 error:(NSError **)error {
  std::vector<webrtc::RtpHeaderExtensionCapability> headerExtensionCapabilities;
  for (RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) *
       extension in extensions) {
    headerExtensionCapabilities.push_back(
        extension.nativeRtpHeaderExtensionCapability);
  }
  webrtc::RTCError nativeError =
      _nativeRtpTransceiver->SetHeaderExtensionsToNegotiate(
          headerExtensionCapabilities);
  BOOL ok = nativeError.ok();
  if (!ok && error) {
    NSDictionary *userInfo = @{
      NSLocalizedDescriptionKey :
          [NSString stringWithCString:nativeError.message()
                             encoding:NSUTF8StringEncoding]
    };
    *error = [NSError errorWithDomain:RTC_CONSTANT_TYPE(RTCRtpTransceiverErrorDomain)
                                 code:static_cast<int>(nativeError.type())
                             userInfo:userInfo];
  }
  return ok;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTC_OBJC_TYPE(RTCRtpTransceiver) {\n  "
                                    @"sender: %@\n  receiver: %@\n}",
                                    _sender,
                                    _receiver];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }
  RTC_OBJC_TYPE(RTCRtpTransceiver) *transceiver =
      (RTC_OBJC_TYPE(RTCRtpTransceiver) *)object;
  return _nativeRtpTransceiver == transceiver.nativeRtpTransceiver;
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeRtpTransceiver.get();
}

#pragma mark - Private

- (webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>)nativeRtpTransceiver {
  return _nativeRtpTransceiver;
}

- (instancetype)initWithFactory:
                    (RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
           nativeRtpTransceiver:
               (webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>)
                   nativeRtpTransceiver {
  NSParameterAssert(factory);
  NSParameterAssert(nativeRtpTransceiver);
  self = [super init];
  if (self) {
    _factory = factory;
    _nativeRtpTransceiver = nativeRtpTransceiver;
    _sender = [[RTC_OBJC_TYPE(RTCRtpSender) alloc]
        initWithFactory:_factory
        nativeRtpSender:nativeRtpTransceiver->sender()];
    _receiver = [[RTC_OBJC_TYPE(RTCRtpReceiver) alloc]
          initWithFactory:_factory
        nativeRtpReceiver:nativeRtpTransceiver->receiver()];
    RTCLogInfo(@"RTC_OBJC_TYPE(RTCRtpTransceiver)(%p): created transceiver: %@",
               self,
               self.description);
  }
  return self;
}

+ (webrtc::RtpTransceiverDirection)nativeRtpTransceiverDirectionFromDirection:
        (RTC_OBJC_TYPE(RTCRtpTransceiverDirection))direction {
  switch (direction) {
    case RTC_OBJC_TYPE(RTCRtpTransceiverDirectionSendRecv):
      return webrtc::RtpTransceiverDirection::kSendRecv;
    case RTC_OBJC_TYPE(RTCRtpTransceiverDirectionSendOnly):
      return webrtc::RtpTransceiverDirection::kSendOnly;
    case RTC_OBJC_TYPE(RTCRtpTransceiverDirectionRecvOnly):
      return webrtc::RtpTransceiverDirection::kRecvOnly;
    case RTC_OBJC_TYPE(RTCRtpTransceiverDirectionInactive):
      return webrtc::RtpTransceiverDirection::kInactive;
    case RTC_OBJC_TYPE(RTCRtpTransceiverDirectionStopped):
      return webrtc::RtpTransceiverDirection::kStopped;
  }
}

+ (RTC_OBJC_TYPE(RTCRtpTransceiverDirection))rtpTransceiverDirectionFromNativeDirection:
        (webrtc::RtpTransceiverDirection)nativeDirection {
  switch (nativeDirection) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return RTC_OBJC_TYPE(RTCRtpTransceiverDirectionSendRecv);
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return RTC_OBJC_TYPE(RTCRtpTransceiverDirectionSendOnly);
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return RTC_OBJC_TYPE(RTCRtpTransceiverDirectionRecvOnly);
    case webrtc::RtpTransceiverDirection::kInactive:
      return RTC_OBJC_TYPE(RTCRtpTransceiverDirectionInactive);
    case webrtc::RtpTransceiverDirection::kStopped:
      return RTC_OBJC_TYPE(RTCRtpTransceiverDirectionStopped);
  }
}

@end
