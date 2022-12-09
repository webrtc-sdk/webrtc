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

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

class DefaultKeyManagerImpl : public webrtc::KeyManager {
 public:
  DefaultKeyManagerImpl() = default;
  ~DefaultKeyManagerImpl() override = default;

  bool SetKey(int index, std::vector<uint8_t> key) {
    if (index > kMaxKeySize) {
      return false;
    }
    webrtc::MutexLock lock(&mutex_);
    if (index > (int)keys_.size()) {
      keys_.resize(index + 1);
    }
    keys_[index] = key;
    return true;
  }

  bool SetKeys(std::vector<std::vector<uint8_t>> keys) {
    webrtc::MutexLock lock(&mutex_);
    keys_ = keys;
    return true;
  }

  const std::vector<std::vector<uint8_t>> keys() const override {
    webrtc::MutexLock lock(&mutex_);
    return keys_;
  }

 private:
  mutable webrtc::Mutex mutex_;
  std::vector<std::vector<uint8_t>> keys_;
};

@implementation RTC_OBJC_TYPE (RTCFrameCryptorKeyManager) {
  std::shared_ptr<DefaultKeyManagerImpl> _nativeKeyManager;
}

-(std::shared_ptr<webrtc::KeyManager>) nativeKeyManager {
  return _nativeKeyManager;
}

- (instancetype)init {
    if (self = [super init]) {
        _nativeKeyManager = std::make_shared<DefaultKeyManagerImpl>();
    }
    return self;
}

- (void)setKey:(NSData*)key forIndex:(int)index {
    _nativeKeyManager->SetKey(index, std::vector<uint8_t>((const uint8_t*)key.bytes, ((const uint8_t*)key.bytes) + key.length));
}

- (void)setKeys:(NSArray<NSData *> *)keys {
    std::vector<std::vector<uint8_t>> nativeKeys;
    for (NSData *key in keys) {
        nativeKeys.push_back(std::vector<uint8_t>((const uint8_t*)key.bytes, ((const uint8_t*)key.bytes) + key.length));
    }
    _nativeKeyManager->SetKeys(nativeKeys);
}

- (NSArray<NSData*> *)keys {
    std::vector<std::vector<uint8_t>> nativeKeys = _nativeKeyManager->keys();
    NSMutableArray<NSData *> *keys = [NSMutableArray array];
    for (std::vector<uint8_t> key : nativeKeys) {
        [keys addObject:[NSData dataWithBytes:key.data() length:key.size()]];
    }
    return keys;
}

@end
