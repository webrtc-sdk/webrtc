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

typedef NS_ENUM(NSUInteger, RTCCyrptorAlgorithm) {
  RTCCyrptorAlgorithmAesGcm = 0,
  RTCCyrptorAlgorithmAesCbc,
};

NS_ASSUME_NONNULL_BEGIN

@class RTC_OBJC_TYPE(RTCRtpSender);
@class RTC_OBJC_TYPE(RTCRtpReceiver);
@class RTC_OBJC_TYPE(RTCFrameCryptorKeyManager);

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCFrameCryptor) : NSObject

@property(nonatomic, assign) BOOL enabled;

@property(nonatomic, assign) int keyIndex;

@property(nonatomic, readonly) NSString *participantId;

- (instancetype)initWithRtpSender:(RTC_OBJC_TYPE(RTCRtpSender) *)sender
                    participantId:(NSString *)participantId
                        algorithm:(RTCCyrptorAlgorithm)algorithm
                       keyManager:(RTC_OBJC_TYPE(RTCFrameCryptorKeyManager) *)keyManager;

- (instancetype)initWithRtpReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
                      participantId:(NSString *)participantId
                          algorithm:(RTCCyrptorAlgorithm)algorithm
                         keyManager:(RTC_OBJC_TYPE(RTCFrameCryptorKeyManager) *)keyManager;

@end

NS_ASSUME_NONNULL_END
