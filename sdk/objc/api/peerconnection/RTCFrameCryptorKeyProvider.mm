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

#import "RTCFrameCryptorKeyProvider+Private.h"

#include <memory>
#include "api/crypto/frame_crypto_transformer.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCFrameCryptorKeyProvider) {
  rtc::scoped_refptr<webrtc::DefaultKeyProviderImpl> _nativeKeyProvider;
}

- (rtc::scoped_refptr<webrtc::KeyProvider>)nativeKeyProvider {
  return _nativeKeyProvider;
}

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(NSData *)uncryptedMagicBytes {
  return [self initWithRatchetSalt:salt
                  ratchetWindowSize:windowSize
                      sharedKeyMode:sharedKey
                uncryptedMagicBytes:uncryptedMagicBytes
                   failureTolerance:-1];
}

- (instancetype)initWithRatchetSalt:(NSData *)salt
                  ratchetWindowSize:(int)windowSize
                      sharedKeyMode:(BOOL)sharedKey
                uncryptedMagicBytes:(nullable NSData *)uncryptedMagicBytes
                   failureTolerance:(int)failureTolerance {
  if (self = [super init]) {
    webrtc::KeyProviderOptions options;
    options.ratchet_salt = std::vector<uint8_t>((const uint8_t *)salt.bytes,
                                                ((const uint8_t *)salt.bytes) + salt.length);
    options.ratchet_window_size = windowSize;
    options.shared_key = sharedKey;
    options.failure_tolerance = failureTolerance;
    if(uncryptedMagicBytes != nil) {
      options.uncrypted_magic_bytes = std::vector<uint8_t>((const uint8_t *)uncryptedMagicBytes.bytes,
                                                          ((const uint8_t *)uncryptedMagicBytes.bytes) + uncryptedMagicBytes.length);
    }
    _nativeKeyProvider = rtc::make_ref_counted<webrtc::DefaultKeyProviderImpl>(options);
  }
  return self;
}

- (void)setKey:(NSData *)key withIndex:(int)index forParticipant:(NSString *)participantId {
  _nativeKeyProvider->SetKey(
      [participantId stdString],
      index,
      std::vector<uint8_t>((const uint8_t *)key.bytes, ((const uint8_t *)key.bytes) + key.length));
}

- (void)setSharedKey:(NSData *)key withIndex:(int)index {
  _nativeKeyProvider->SetSharedKey(
      index,
      std::vector<uint8_t>((const uint8_t *)key.bytes, ((const uint8_t *)key.bytes) + key.length));
}

- (NSData *)ratchetSharedKey:(int)index {
  std::vector<uint8_t> nativeKey = _nativeKeyProvider->RatchetSharedKey(index);
  return [NSData dataWithBytes:nativeKey.data() length:nativeKey.size()];
}

- (NSData *)exportSharedKey:(int)index {
  std::vector<uint8_t> nativeKey = _nativeKeyProvider->ExportSharedKey(index);
  return [NSData dataWithBytes:nativeKey.data() length:nativeKey.size()];
}

- (NSData *)ratchetKey:(NSString *)participantId withIndex:(int)index {
  std::vector<uint8_t> nativeKey = _nativeKeyProvider->RatchetKey([participantId stdString], index);
  return [NSData dataWithBytes:nativeKey.data() length:nativeKey.size()];
}

- (NSData *)exportKey:(NSString *)participantId withIndex:(int)index {
  std::vector<uint8_t> nativeKey = _nativeKeyProvider->ExportKey([participantId stdString], index);
  return [NSData dataWithBytes:nativeKey.data() length:nativeKey.size()];
}

- (void)setSifTrailer:(NSData *)trailer {
  _nativeKeyProvider->SetSifTrailer(
      std::vector<uint8_t>((const uint8_t *)trailer.bytes,
                           ((const uint8_t *)trailer.bytes) + trailer.length));
}

@end
