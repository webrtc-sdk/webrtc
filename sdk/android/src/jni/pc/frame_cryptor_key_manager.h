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

#ifndef SDK_ANDROID_SRC_JNI_PC_FRAME_CRYPTOR_KEY_MANAGER_H_
#define SDK_ANDROID_SRC_JNI_PC_FRAME_CRYPTOR_KEY_MANAGER_H_

#include <jni.h>

#include "api/crypto/frame_crypto_transformer.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {
namespace jni {

class DefaultKeyManagerImpl : public webrtc::KeyManager {
 public:
  DefaultKeyManagerImpl() = default;
  ~DefaultKeyManagerImpl() override = default;

  bool SetKey(int index, std::vector<int8_t> key) {
    std::vector<uint8_t> uint8Key = std::vector<uint8_t>(key.begin(), key.end());
    return SetKey(index, uint8Key);
  }

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

  const std::vector<uint8_t> GetKey(int index) const {
    webrtc::MutexLock lock(&mutex_);
    if (index >= (int)keys_.size()) {
      return std::vector<uint8_t>();
    }
    return keys_[index];
  }

  int KeyCount() const {
    webrtc::MutexLock lock(&mutex_);
    return keys_.size();
  }

 private:
  mutable webrtc::Mutex mutex_;
  std::vector<std::vector<uint8_t>> keys_;
};

ScopedJavaLocalRef<jobject> NativeToJavaFrameCryptorKeyManager(
    JNIEnv* env,
    rtc::scoped_refptr<DefaultKeyManagerImpl> cryptor);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_FRAME_CRYPTOR_KEY_MANAGER_H_
