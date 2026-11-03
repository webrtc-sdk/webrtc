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

typedef NS_ENUM(NSUInteger, RTC_OBJC_TYPE(RTCKeyDerivationAlgorithm)) {
  RTC_OBJC_TYPE(RTCKeyDerivationAlgorithmPBKDF2) = 0,
  RTC_OBJC_TYPE(RTCKeyDerivationAlgorithmHKDF),
};

/** AES-GCM key size used for frame and data packet encryption. */
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCFrameCryptorKeySize)) {
  RTC_OBJC_TYPE(RTCFrameCryptorKeySizeAES128) = 128,
  RTC_OBJC_TYPE(RTCFrameCryptorKeySizeAES256) = 256,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCFrameCryptorKeyProvider) : NSObject

- (void)setSharedKey:(NSData *)key withIndex:(int)index;

- (NSData *)ratchetSharedKey:(int)index;

- (NSData *)exportSharedKey:(int)index;

- (void)setKey:(NSData *)key withIndex:(int)index forParticipant:(NSString *)participantId;

- (NSData *)ratchetKey:(NSString *)participantId withIndex:(int)index;

- (NSData *)exportKey:(NSString *)participantId withIndex:(int)index;

- (void)setSifTrailer:(NSData *)trailer;

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes;

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes
                   failureTolerance:(int)failureTolerance
                        keyRingSize:(int)keyRingSize;

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes
                   failureTolerance:(int)failureTolerance
                        keyRingSize:(int)keyRingSize
    discardFrameWhenCryptorNotReady:(BOOL)discardFrameWhenCryptorNotReady;    

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes
                   failureTolerance:(int)failureTolerance
                        keyRingSize:(int)keyRingSize
    discardFrameWhenCryptorNotReady:(BOOL)discardFrameWhenCryptorNotReady
             keyDerivationAlgorithm:(RTC_OBJC_TYPE(RTCKeyDerivationAlgorithm))keyDerivationAlgorithm;

/** Every participant must use the same key size. Other initializers default to AES-128. */
- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes
                   failureTolerance:(int)failureTolerance
                        keyRingSize:(int)keyRingSize
    discardFrameWhenCryptorNotReady:(BOOL)discardFrameWhenCryptorNotReady
             keyDerivationAlgorithm:(RTC_OBJC_TYPE(RTCKeyDerivationAlgorithm))keyDerivationAlgorithm
                            keySize:(RTC_OBJC_TYPE(RTCFrameCryptorKeySize))keySize;

@end

NS_ASSUME_NONNULL_END
