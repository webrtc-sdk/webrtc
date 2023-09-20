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
  rtc::scoped_refptr<webrtc::FrameCryptorTransformer> frame_crypto_transformer_;
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

- (instancetype)initWithRtpSender:(RTC_OBJC_TYPE(RTCRtpSender) *)sender
                    participantId:(NSString *)participantId
                        algorithm:(RTCCyrptorAlgorithm)algorithm
                       keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) { 
    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;
    auto rtpSender = sender.nativeRtpSender;
    auto mediaType = rtpSender->track()->kind() == "audio" ?
        webrtc::FrameCryptorTransformer::MediaType::kAudioFrame :
        webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
    frame_crypto_transformer_ = rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
        new webrtc::FrameCryptorTransformer([participantId stdString],
                                            mediaType,
                                            [self algorithmFromEnum:algorithm],
                                            keyProvider.nativeKeyProvider));

    rtpSender->SetEncoderToPacketizerFrameTransformer(frame_crypto_transformer_);
    frame_crypto_transformer_->SetEnabled(false);
    frame_crypto_transformer_->RegisterFrameCryptorTransformerObserver(_observer);
  }
  return self;
}

- (instancetype)initWithRtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                      participantId:(NSString *)participantId
                          algorithm:(RTCCyrptorAlgorithm)algorithm
                         keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) {
    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;
    auto rtpReceiver = receiver.nativeRtpReceiver;
    auto mediaType = rtpReceiver->track()->kind() == "audio" ?
        webrtc::FrameCryptorTransformer::MediaType::kAudioFrame :
        webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
    frame_crypto_transformer_ = rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
        new webrtc::FrameCryptorTransformer([participantId stdString],
                                            mediaType,
                                            [self algorithmFromEnum:algorithm],
                                            keyProvider.nativeKeyProvider));

    rtpReceiver->SetDepacketizerToDecoderFrameTransformer(frame_crypto_transformer_);
    frame_crypto_transformer_->SetEnabled(false);
    frame_crypto_transformer_->RegisterFrameCryptorTransformerObserver(_observer);
  }
  return self;
}

- (BOOL)enabled {
  return frame_crypto_transformer_->enabled();
}

- (void)setEnabled:(BOOL)enabled {
  frame_crypto_transformer_->SetEnabled(enabled);
}

- (int)keyIndex {
  return frame_crypto_transformer_->key_index();
}

- (void)setKeyIndex:(int)keyIndex {
  frame_crypto_transformer_->SetKeyIndex(keyIndex);
}

- (void)dealloc {
  frame_crypto_transformer_->UnRegisterFrameCryptorTransformerObserver();
}

@end
