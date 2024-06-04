/*
 * Copyright 2022 LiveKit
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

#import "RTCFrameCryptor+Private.h"
#import "RTCFrameCryptorKeyProvider+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"

#include <memory>

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/crypto/frame_crypto_transformer.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"

namespace webrtc {

RTCFrameCryptorDelegateAdapter::RTCFrameCryptorDelegateAdapter(RTC_OBJC_TYPE(RTCFrameCryptor) *
                                                               frameCryptor)
    : frame_cryptor_(frameCryptor) {}

RTCFrameCryptorDelegateAdapter::~RTCFrameCryptorDelegateAdapter() {}

/*
  kNew = 0,
  kOk,
  kEncryptionFailed,
  kDecryptionFailed,
  kMissingKey,
  kInternalError,
*/
void RTCFrameCryptorDelegateAdapter::OnFrameCryptionStateChanged(const std::string participant_id,
                                                                 FrameCryptionState state) {
  RTC_OBJC_TYPE(RTCFrameCryptor) *frameCryptor = frame_cryptor_;
  if (frameCryptor.delegate) {
    switch (state) {
      case FrameCryptionState::kNew:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateNew];
        break;
      case FrameCryptionState::kOk:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateOk];
        break;
      case FrameCryptionState::kEncryptionFailed:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateEncryptionFailed];
        break;
      case FrameCryptionState::kDecryptionFailed:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateDecryptionFailed];
        break;
      case FrameCryptionState::kMissingKey:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateMissingKey];
        break;
      case FrameCryptionState::kKeyRatcheted:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateKeyRatcheted];
        break;
      case FrameCryptionState::kInternalError:
        [frameCryptor.delegate frameCryptor:frameCryptor
            didStateChangeWithParticipantId:[NSString stringForStdString:participant_id]
                                  withState:FrameCryptionStateInternalError];
        break;
    }
  }
}
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCFrameCryptor) {
  const webrtc::RtpSenderInterface *_sender;
  const webrtc::RtpReceiverInterface *_receiver;
  NSString *_participantId;
  rtc::scoped_refptr<webrtc::FrameCryptorTransformer> _frame_crypto_transformer;
  rtc::scoped_refptr<webrtc::RTCFrameCryptorDelegateAdapter> _observer;
}

@synthesize participantId = _participantId;
@synthesize delegate = _delegate;

- (webrtc::FrameCryptorTransformer::Algorithm)algorithmFromEnum:(RTCCyrptorAlgorithm)algorithm {
  switch (algorithm) {
    case RTCCyrptorAlgorithmAesGcm:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
    case RTCCyrptorAlgorithmAesCbc:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesCbc;
    default:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
  }
}

- (nullable instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                               rtpSender:(RTC_OBJC_TYPE(RTCRtpSender) *)sender
                           participantId:(NSString *)participantId
                               algorithm:(RTCCyrptorAlgorithm)algorithm
                             keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) {
    rtc::scoped_refptr<webrtc::RtpSenderInterface> nativeRtpSender = sender.nativeRtpSender;
    if (nativeRtpSender == nullptr) return nil;

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack = nativeRtpSender->track();
    if (nativeTrack == nullptr) return nil;

    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;

    webrtc::FrameCryptorTransformer::MediaType mediaType =
        nativeTrack->kind() == "audio" ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
                                       : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;

    _frame_crypto_transformer =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            factory.signalingThread, [participantId stdString], mediaType,
            [self algorithmFromEnum:algorithm], keyProvider.nativeKeyProvider));

    nativeRtpSender->SetEncoderToPacketizerFrameTransformer(_frame_crypto_transformer);
    _frame_crypto_transformer->SetEnabled(false);
    _frame_crypto_transformer->RegisterFrameCryptorTransformerObserver(_observer);
  }
  return self;
}

- (nullable instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                             rtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                           participantId:(NSString *)participantId
                               algorithm:(RTCCyrptorAlgorithm)algorithm
                             keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) {
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> nativeRtpReceiver = receiver.nativeRtpReceiver;
    if (nativeRtpReceiver == nullptr) return nil;

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack = nativeRtpReceiver->track();
    if (nativeTrack == nullptr) return nil;

    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;

    webrtc::FrameCryptorTransformer::MediaType mediaType =
        nativeTrack->kind() == "audio" ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
                                       : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;

    _frame_crypto_transformer =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            factory.signalingThread, [participantId stdString], mediaType,
            [self algorithmFromEnum:algorithm], keyProvider.nativeKeyProvider));

    nativeRtpReceiver->SetDepacketizerToDecoderFrameTransformer(_frame_crypto_transformer);
    _frame_crypto_transformer->SetEnabled(false);
    _frame_crypto_transformer->RegisterFrameCryptorTransformerObserver(_observer);
  }
  return self;
}

- (void)dealloc {
  if (_frame_crypto_transformer == nullptr) return;
  _frame_crypto_transformer->UnRegisterFrameCryptorTransformerObserver();
}

- (BOOL)enabled {
  return _frame_crypto_transformer->enabled();
}

- (void)setEnabled:(BOOL)enabled {
  _frame_crypto_transformer->SetEnabled(enabled);
}

- (int)keyIndex {
  return _frame_crypto_transformer->key_index();
}

- (void)setKeyIndex:(int)keyIndex {
  _frame_crypto_transformer->SetKeyIndex(keyIndex);
}

@end
