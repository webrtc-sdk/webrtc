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

  /// Set the key at the given index.
  bool SetKey(const std::string participant_id,
              int index,
              std::vector<uint8_t> key) {
    if (index > webrtc::KeyManager::kMaxKeySize) {
      return false;
    }

    if (keys_.find(participant_id) == keys_.end()) {
      keys_[participant_id] = std::vector<std::vector<uint8_t>>();
    }

    webrtc::MutexLock lock(&mutex_);
    if (index + 1 > (int)keys_[participant_id].size()) {
      keys_[participant_id].resize(index + 1);
    }
    keys_[participant_id][index] = key;
    return true;
  }

  /// Set the keys.
  bool SetKeys(const std::string participant_id,
               std::vector<std::vector<uint8_t>> keys) {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      keys_[participant_id] = std::vector<std::vector<uint8_t>>();
    }

    keys_[participant_id].clear();
    for (auto key : keys) {
      keys_[participant_id].push_back(key);
    }
    return true;
  }

  const std::vector<std::vector<uint8_t>> GetKeys(
      const std::string participant_id) const {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<std::vector<uint8_t>>();
    }

    return keys_.find(participant_id)->second;
  }

  const std::vector<std::vector<uint8_t>> keys(
      const std::string participant_id) const override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<std::vector<uint8_t>>();
    }

    return keys_.find(participant_id)->second;
  }

 private:
  mutable webrtc::Mutex mutex_;
  std::map<std::string, std::vector<std::vector<uint8_t>>> keys_;
};

ScopedJavaLocalRef<jobject> NativeToJavaFrameCryptorKeyManager(
    JNIEnv* env,
    rtc::scoped_refptr<DefaultKeyManagerImpl> cryptor);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_FRAME_CRYPTOR_KEY_MANAGER_H_
