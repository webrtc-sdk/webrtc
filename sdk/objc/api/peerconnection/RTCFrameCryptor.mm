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

#import <os/lock.h>
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
  rtc::scoped_refptr<webrtc::FrameCryptorTransformer> _frame_crypto_transformer;
  rtc::scoped_refptr<webrtc::RTCFrameCryptorDelegateAdapter> _observer;
  os_unfair_lock _lock;
}

@synthesize participantId = _participantId;
@synthesize delegate = _delegate;

- (webrtc::FrameCryptorTransformer::Algorithm)algorithmFromEnum:(RTCCryptorAlgorithm)algorithm {
  switch (algorithm) {
    case RTCCryptorAlgorithmAesGcm:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
    default:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
  }
}

- (nullable instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                               rtpSender:(RTC_OBJC_TYPE(RTCRtpSender) *)sender
                           participantId:(NSString *)participantId
                               algorithm:(RTCCryptorAlgorithm)algorithm
                             keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) {
    _lock = OS_UNFAIR_LOCK_INIT;

    rtc::scoped_refptr<webrtc::RtpSenderInterface> nativeRtpSender = sender.nativeRtpSender;
    if (nativeRtpSender == nullptr) return nil;

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack = nativeRtpSender->track();
    if (nativeTrack == nullptr) return nil;

    webrtc::FrameCryptorTransformer::MediaType mediaType =
        nativeTrack->kind() == "audio" ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
                                       : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;

    os_unfair_lock_lock(&_lock);
    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;

    _frame_crypto_transformer =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            factory.signalingThread, [participantId stdString], mediaType,
            [self algorithmFromEnum:algorithm], keyProvider.nativeKeyProvider));

    factory.workerThread->BlockingCall([self, nativeRtpSender] {
      // Must be called on Worker thread
      nativeRtpSender->SetEncoderToPacketizerFrameTransformer(_frame_crypto_transformer);
    });

    _frame_crypto_transformer->SetEnabled(false);
    _frame_crypto_transformer->RegisterFrameCryptorTransformerObserver(_observer);
    os_unfair_lock_unlock(&_lock);
  }

  return self;
}

- (nullable instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                             rtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                           participantId:(NSString *)participantId
                               algorithm:(RTCCryptorAlgorithm)algorithm
                             keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  if (self = [super init]) {
    _lock = OS_UNFAIR_LOCK_INIT;

    rtc::scoped_refptr<webrtc::RtpReceiverInterface> nativeRtpReceiver = receiver.nativeRtpReceiver;
    if (nativeRtpReceiver == nullptr) return nil;

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack = nativeRtpReceiver->track();
    if (nativeTrack == nullptr) return nil;

    webrtc::FrameCryptorTransformer::MediaType mediaType =
        nativeTrack->kind() == "audio" ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
                                       : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;

    os_unfair_lock_lock(&_lock);
    _observer = rtc::make_ref_counted<webrtc::RTCFrameCryptorDelegateAdapter>(self);
    _participantId = participantId;

    _frame_crypto_transformer =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            factory.signalingThread, [participantId stdString], mediaType,
            [self algorithmFromEnum:algorithm], keyProvider.nativeKeyProvider));

    factory.workerThread->BlockingCall([self, nativeRtpReceiver] {
      // Must be called on Worker thread
      nativeRtpReceiver->SetDepacketizerToDecoderFrameTransformer(_frame_crypto_transformer);
    });

    _frame_crypto_transformer->SetEnabled(false);
    _frame_crypto_transformer->RegisterFrameCryptorTransformerObserver(_observer);
    os_unfair_lock_unlock(&_lock);
  }

  return self;
}

- (void)dealloc {
  os_unfair_lock_lock(&_lock);
  if (_frame_crypto_transformer != nullptr) {
    _frame_crypto_transformer->UnRegisterFrameCryptorTransformerObserver();
    _frame_crypto_transformer = nullptr;
  }
  _observer = nullptr;
  os_unfair_lock_unlock(&_lock);
}

- (BOOL)enabled {
  os_unfair_lock_lock(&_lock);
  BOOL result = _frame_crypto_transformer != nullptr ? _frame_crypto_transformer->enabled() : NO;
  os_unfair_lock_unlock(&_lock);
  return result;
}

- (void)setEnabled:(BOOL)enabled {
  os_unfair_lock_lock(&_lock);
  if (_frame_crypto_transformer != nullptr) {
    _frame_crypto_transformer->SetEnabled(enabled);
  }
  os_unfair_lock_unlock(&_lock);
}

- (int)keyIndex {
  os_unfair_lock_lock(&_lock);
  int result = _frame_crypto_transformer != nullptr ? _frame_crypto_transformer->key_index() : 0;
  os_unfair_lock_unlock(&_lock);
  return result;
}

- (void)setKeyIndex:(int)keyIndex {
  os_unfair_lock_lock(&_lock);
  if (_frame_crypto_transformer != nullptr) {
    _frame_crypto_transformer->SetKeyIndex(keyIndex);
  }
  os_unfair_lock_unlock(&_lock);
}

@end
