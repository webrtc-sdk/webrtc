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

#import "RTCFrameCryptorKeyManager+Private.h"

#include <memory>
#include <unordered_map>
#include "api/crypto/frame_crypto_transformer.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCFrameCryptorKeyManager) {
  rtc::scoped_refptr<webrtc::DefaultKeyManagerImpl> _nativeKeyManager;
}

- (rtc::scoped_refptr<webrtc::KeyManager>)nativeKeyManager {
  return _nativeKeyManager;
}

- (instancetype)init {
  if (self = [super init]) {
    _nativeKeyManager = rtc::make_ref_counted<webrtc::DefaultKeyManagerImpl>();
  }
  return self;
}

- (void)setKey:(NSData *)key withIndex:(int)index forParticipant:(NSString *)participantId {
  _nativeKeyManager->SetKey(
      [participantId stdString],
      index,
      std::vector<uint8_t>((const uint8_t *)key.bytes, ((const uint8_t *)key.bytes) + key.length));
}

- (void)setKeys:(NSArray<NSData *> *)keys forParticipant:(NSString *)participantId {
  std::vector<std::vector<uint8_t>> nativeKeys;
  for (NSData *key in keys) {
    nativeKeys.push_back(std::vector<uint8_t>((const uint8_t *)key.bytes,
                                              ((const uint8_t *)key.bytes) + key.length));
  }
  _nativeKeyManager->SetKeys([participantId stdString], nativeKeys);
}

- (NSArray<NSData *> *)getKeys:(NSString *)participantId {
  std::vector<std::vector<uint8_t>> nativeKeys =
      _nativeKeyManager->keys([participantId stdString]);
  NSMutableArray<NSData *> *keys = [NSMutableArray array];
  for (std::vector<uint8_t> key : nativeKeys) {
    [keys addObject:[NSData dataWithBytes:key.data() length:key.size()]];
  }
  return keys;
}

@end
