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

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

class DefaultKeyManagerImpl : public webrtc::KeyManager {
 public:
  DefaultKeyManagerImpl() = default;
  ~DefaultKeyManagerImpl() override = default;

  /// Set the key at the given index.
  bool SetKey(const std::string participant_id, int index, std::vector<uint8_t> key) {
    webrtc::MutexLock lock(&mutex_);

    if (keys_.find(participant_id) == keys_.end()) {
      keys_[participant_id] = std::vector<std::vector<uint8_t>>();
    }

    if (index + 1 > (int)keys_[participant_id].size()) {
      keys_[participant_id].resize(index + 1);
    }
    keys_[participant_id][index] = key;
    return true;
  }

  /// Set the keys.
  bool SetKeys(const std::string participant_id, std::vector<std::vector<uint8_t>> keys) {
    webrtc::MutexLock lock(&mutex_);
    keys_[participant_id] = keys;
    return true;
  }

  const std::vector<std::vector<uint8_t>> keys(const std::string participant_id) const override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<std::vector<uint8_t>>();
    }

    return keys_.find(participant_id)->second;
  }

 private:
  mutable webrtc::Mutex mutex_;
  std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> keys_;
};

@implementation RTC_OBJC_TYPE (RTCFrameCryptorKeyManager) {
  rtc::scoped_refptr<DefaultKeyManagerImpl> _nativeKeyManager;
}

- (rtc::scoped_refptr<webrtc::KeyManager>)nativeKeyManager {
  return _nativeKeyManager;
}

- (instancetype)init {
  if (self = [super init]) {
    _nativeKeyManager = rtc::make_ref_counted<DefaultKeyManagerImpl>();
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
