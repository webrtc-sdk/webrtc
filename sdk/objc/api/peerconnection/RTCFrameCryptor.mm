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
#import "RTCFrameCryptorKeyManager+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"

#include <memory>

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/crypto/frame_crypto_transformer.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"

@implementation RTC_OBJC_TYPE (RTCFrameCryptor) {
  const webrtc::RtpSenderInterface *_sender;
  const webrtc::RtpReceiverInterface *_receiver;
  rtc::scoped_refptr<webrtc::FrameCryptorTransformer> frame_crypto_transformer_;
}

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
                        algorithm:(RTCCyrptorAlgorithm)algorithm
                       keyManager:(RTC_OBJC_TYPE(RTCFrameCryptorKeyManager) *)keyManager {
  if (self = [super init]) {
    auto rtpSender = sender.nativeRtpSender;
    auto mediaType = rtpSender->track()->kind() == "audio" ?
        webrtc::FrameCryptorTransformer::MediaType::kAudioFrame :
        webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
    frame_crypto_transformer_ =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            mediaType, [self algorithmFromEnum:algorithm], keyManager.nativeKeyManager));

    rtpSender->SetEncoderToPacketizerFrameTransformer(frame_crypto_transformer_);
    frame_crypto_transformer_->SetEnabled(false);
  }
  return self;
}

- (instancetype)initWithRtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                          algorithm:(RTCCyrptorAlgorithm)algorithm
                         keyManager:(RTC_OBJC_TYPE(RTCFrameCryptorKeyManager) *)keyManager {
  if (self = [super init]) {
    auto rtpReceiver = receiver.nativeRtpReceiver;
    auto mediaType = rtpReceiver->track()->kind() == "audio" ?
        webrtc::FrameCryptorTransformer::MediaType::kAudioFrame :
        webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
    frame_crypto_transformer_ =
        rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(new webrtc::FrameCryptorTransformer(
            mediaType, [self algorithmFromEnum:algorithm], keyManager.nativeKeyManager));

    rtpReceiver->SetDepacketizerToDecoderFrameTransformer(frame_crypto_transformer_);
    frame_crypto_transformer_->SetEnabled(false);
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

@end
