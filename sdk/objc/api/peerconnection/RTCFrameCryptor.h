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

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class RTC_OBJC_TYPE(RTCRtpSender);
@class RTC_OBJC_TYPE(RTCRtpReceiver);
@class RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider);
@class RTC_OBJC_TYPE(RTCFrameCryptor);

typedef NS_ENUM(NSUInteger, RTCCyrptorAlgorithm) {
  RTCCyrptorAlgorithmAesGcm = 0,
  RTCCyrptorAlgorithmAesCbc,
};

typedef NS_ENUM(NSInteger, FrameCryptionState) {
  FrameCryptionStateNew = 0,
  FrameCryptionStateOk,
  FrameCryptionStateEncryptionFailed,
  FrameCryptionStateDecryptionFailed,
  FrameCryptionStateMissingKey,
  FrameCryptionStateKeyRatcheted,
  FrameCryptionStateInternalError,
};

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCFrameCryptorDelegate)<NSObject>
    /** Called when the RTCFrameCryptor got errors. */
    - (void)frameCryptor
    : (RTC_OBJC_TYPE(RTCFrameCryptor) *)frameCryptor didStateChangeWithParticipantId
    : (NSString *)participantId withState : (FrameCryptionState)stateChanged;
@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCFrameCryptor) : NSObject

@property(nonatomic, assign) BOOL enabled;

@property(nonatomic, assign) int keyIndex;

@property(nonatomic, readonly) NSString *participantId;

@property(nonatomic, weak, nullable) id<RTC_OBJC_TYPE(RTCFrameCryptorDelegate)> delegate;

- (instancetype)initWithRtpSender:(RTC_OBJC_TYPE(RTCRtpSender) *)sender
                    participantId:(NSString *)participantId
                        algorithm:(RTCCyrptorAlgorithm)algorithm
                       keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider;

- (instancetype)initWithRtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                      participantId:(NSString *)participantId
                          algorithm:(RTCCyrptorAlgorithm)algorithm
                         keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider;

@end

NS_ASSUME_NONNULL_END
