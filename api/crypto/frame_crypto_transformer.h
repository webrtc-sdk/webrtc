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

#ifndef WEBRTC_FRAME_CRYPTOR_TRANSFORMER_H_
#define WEBRTC_FRAME_CRYPTOR_TRANSFORMER_H_

#include <unordered_map>

#include "api/frame_transformer_interface.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace webrtc {

int DeriveAesKeyFromRawKey(const std::vector<uint8_t> raw_key,
                           const std::vector<uint8_t>& salt,
                           unsigned int optional_length_bits,
                           std::vector<uint8_t>* derived_key);

const size_t KEYRING_SIZE = 16;

class ParticipantKeyHandler {
 public:
  struct KeySet {
    std::vector<uint8_t> material;
    std::vector<uint8_t> encryptionKey;
    KeySet(std::vector<uint8_t> material, std::vector<uint8_t> encryptionKey)
        : material(material), encryptionKey(encryptionKey) {}
  };

 public:
  int currentKeyIndex = 0;
  std::vector<std::shared_ptr<KeySet>> cryptoKeyRing_;
  bool enabled_;

  ParticipantKeyHandler(bool enabled, KeyManager::KeyProviderOptions options)
      : enabled_(enabled), options_(options) {
    cryptoKeyRing_.resize(KEYRING_SIZE);
  }

  bool ratchetKey(int keyIndex) {
    std::shared_ptr<KeySet> currentMaterial = getKey(keyIndex);
    std::shared_ptr<KeySet> newMaterial =
        deriveBits(currentMaterial->encryptionKey, options_.ratchetSalt);
    setKeyFromMaterial(newMaterial->encryptionKey,
                       keyIndex != -1 ? keyIndex : currentKeyIndex);
  }

  std::shared_ptr<KeySet> getKey(int keyIndex) {
    return cryptoKeyRing_[keyIndex != -1 ? keyIndex : currentKeyIndex];
  }

  void setKeyFromMaterial(std::vector<uint8_t> password, int keyIndex) {
    if (keyIndex >= 0) {
      currentKeyIndex = keyIndex % cryptoKeyRing_.size();
    }
    cryptoKeyRing_[currentKeyIndex] =
        deriveBits(password, options_.ratchetSalt);
  }

  std::shared_ptr<KeySet> deriveBits(std::vector<uint8_t> password,
                                     std::vector<uint8_t> ratchetSalt) {
    std::vector<uint8_t> derived_key;
    if (DeriveAesKeyFromRawKey(password, ratchetSalt, 256, &derived_key) == 0) {
      return std::make_shared<KeySet>(password, derived_key);
    }
    return nullptr;
  }

 private:
  KeyManager::KeyProviderOptions options_;
};

class KeyManager : public rtc::RefCountInterface {
 public:
  enum { kRawKeySize = 32 };

  struct KeyProviderOptions {
    bool sharedKey;
    std::vector<uint8_t> ratchetSalt;
    int ratchetWindowSize;
  };

 public:
  virtual const std::vector<std::vector<uint8_t>> keys(
      const std::string participant_id) const = 0;

  virtual void ratchetKey(const std::string participant_id, int keyIndex) = 0;

 protected:
  virtual ~KeyManager() {}

 private:
  KeyProviderOptions options_;
};

class DefaultKeyManagerImpl : public KeyManager {
 public:
  DefaultKeyManagerImpl() = default;
  ~DefaultKeyManagerImpl() override = default;

  /// Set the key at the given index.
  bool SetKey(const std::string participant_id,
              int index,
              std::vector<uint8_t> key) {
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
  bool SetKeys(const std::string participant_id,
               std::vector<std::vector<uint8_t>> keys) {
    webrtc::MutexLock lock(&mutex_);
    keys_[participant_id] = keys;
    return true;
  }

  const std::vector<std::vector<uint8_t>> keys(
      const std::string participant_id) const override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<std::vector<uint8_t>>();
    }

    return keys_.find(participant_id)->second;
  }

  virtual void ratchetKey(const std::string participant_id,
                          int keyIndex) override {}

 private:
 private:
  mutable webrtc::Mutex mutex_;
  std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> keys_;
};

enum FrameCryptionError {
  kNew = 0,
  kOk,
  kEncryptionFailed,
  kDecryptionFailed,
  kMissingKey,
  kInternalError,
};

class FrameCryptorTransformerObserver {
 public:
  virtual void OnFrameCryptionError(const std::string participant_id,
                                    FrameCryptionError error) = 0;

 protected:
  virtual ~FrameCryptorTransformerObserver() {}
};

class RTC_EXPORT FrameCryptorTransformer
    : public rtc::RefCountedObject<webrtc::FrameTransformerInterface> {
 public:
  enum class MediaType {
    kAudioFrame = 0,
    kVideoFrame,
  };

  enum class Algorithm {
    kAesGcm = 0,
    kAesCbc,
  };

  explicit FrameCryptorTransformer(const std::string participant_id,
                                   MediaType type,
                                   Algorithm algorithm,
                                   rtc::scoped_refptr<KeyManager> key_manager);

  virtual void SetFrameCryptorTransformerObserver(
      FrameCryptorTransformerObserver* observer) {
    webrtc::MutexLock lock(&mutex_);
    observer_ = observer;
  }

  virtual void SetKeyIndex(int index) {
    webrtc::MutexLock lock(&mutex_);
    key_index_ = index;
  }

  virtual int key_index() const { return key_index_; };
  virtual void SetEnabled(bool enabled) {
    webrtc::MutexLock lock(&mutex_);
    enabled_cryption_ = enabled;
  }
  virtual bool enabled() const {
    webrtc::MutexLock lock(&mutex_);
    return enabled_cryption_;
  };
  virtual const std::string participant_id() const { return participant_id_; }

 protected:
  virtual void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) override {
    webrtc::MutexLock lock(&sink_mutex_);
    sink_callback_ = callback;
  }
  virtual void UnregisterTransformedFrameCallback() override {
    webrtc::MutexLock lock(&sink_mutex_);
    sink_callback_ = nullptr;
  }
  virtual void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
      uint32_t ssrc) override {
    webrtc::MutexLock lock(&sink_mutex_);
    sink_callbacks_[ssrc] = callback;
  }
  virtual void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
    webrtc::MutexLock lock(&sink_mutex_);
    auto it = sink_callbacks_.find(ssrc);
    if (it != sink_callbacks_.end()) {
      sink_callbacks_.erase(it);
    }
  }

  virtual void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override;

 private:
  void encryptFrame(std::unique_ptr<webrtc::TransformableFrameInterface> frame);
  void decryptFrame(std::unique_ptr<webrtc::TransformableFrameInterface> frame);
  rtc::Buffer makeIv(uint32_t ssrc, uint32_t timestamp);
  uint8_t getIvSize();

 private:
  std::string participant_id_;
  mutable webrtc::Mutex mutex_;
  mutable webrtc::Mutex sink_mutex_;
  bool enabled_cryption_ RTC_GUARDED_BY(mutex_) = false;
  MediaType type_;
  Algorithm algorithm_;
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink_callback_;
  std::map<uint32_t, rtc::scoped_refptr<webrtc::TransformedFrameCallback>>
      sink_callbacks_;
  int key_index_ = 0;
  std::map<uint32_t, uint32_t> sendCounts_;
  rtc::scoped_refptr<KeyManager> key_manager_;
  FrameCryptorTransformerObserver* observer_ = nullptr;
  std::unique_ptr<rtc::Thread> thread_;
  FrameCryptionError last_enc_error_ = FrameCryptionError::kNew;
  FrameCryptionError last_dec_error_ = FrameCryptionError::kNew;
};

}  // namespace webrtc

#endif  // WEBRTC_FRAME_CRYPTOR_TRANSFORMER_H_
