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

int DerivePBKDF2KeyFromRawKey(const std::vector<uint8_t> raw_key,
                              const std::vector<uint8_t>& salt,
                              unsigned int optional_length_bits,
                              std::vector<uint8_t>* derived_key);

namespace webrtc {

const size_t KEYRING_SIZE = 16;

struct KeyProviderOptions {
  bool shared_key;
  std::vector<uint8_t> ratchet_salt;
  std::vector<uint8_t> uncrypted_magic_bytes;
  int ratchet_window_size;
  KeyProviderOptions() : shared_key(false), ratchet_window_size(0) {}
  KeyProviderOptions(KeyProviderOptions& copy)
      : shared_key(copy.shared_key),
        ratchet_salt(copy.ratchet_salt),
        uncrypted_magic_bytes(copy.uncrypted_magic_bytes),
        ratchet_window_size(copy.ratchet_window_size) {}
};

class ParticipantKeyHandler {
 friend class FrameCryptorTransformer;
 public:
  struct KeySet {
    std::vector<uint8_t> material;
    std::vector<uint8_t> encryption_key;
    KeySet(std::vector<uint8_t> material, std::vector<uint8_t> encryptionKey)
        : material(material), encryption_key(encryptionKey) {}
  };

 public:
  ParticipantKeyHandler(KeyProviderOptions options) : options_(options) {
    crypto_key_ring_.resize(KEYRING_SIZE);
  }

  virtual ~ParticipantKeyHandler() = default;

  virtual std::vector<uint8_t> RatchetKey(int key_index) {
    auto current_material = GetKeySet(key_index)->material;
    std::vector<uint8_t> new_material;
    if (DerivePBKDF2KeyFromRawKey(current_material, options_.ratchet_salt, 256,
                                  &new_material) != 0) {
      return std::vector<uint8_t>();
    }
    SetKeyFromMaterial(new_material,
                       key_index != -1 ? key_index : current_key_index_);
    return new_material;
  }

  virtual std::shared_ptr<KeySet> GetKeySet(int key_index) {
    return crypto_key_ring_[key_index != -1 ? key_index : current_key_index_];
  }

  virtual void SetKey(std::vector<uint8_t> password, int key_index) {
    SetKeyFromMaterial(password, key_index);
    have_valid_key = true;
  }

  virtual void SetKeyFromMaterial(std::vector<uint8_t> password, int key_index) {
    if (key_index >= 0) {
      current_key_index_ = key_index % crypto_key_ring_.size();
    }
    crypto_key_ring_[current_key_index_] =
        DeriveKeys(password, options_.ratchet_salt, 128);
  }

  virtual KeyProviderOptions& options() { return options_; }

  std::shared_ptr<KeySet> DeriveKeys(std::vector<uint8_t> password,
                                     std::vector<uint8_t> ratchet_salt,
                                     unsigned int optional_length_bits) {
    std::vector<uint8_t> derived_key;
    if (DerivePBKDF2KeyFromRawKey(password, ratchet_salt, optional_length_bits,
                                  &derived_key) == 0) {
      return std::make_shared<KeySet>(password, derived_key);
    }
    return nullptr;
  }

  std::vector<uint8_t> RatchetKeyMaterial(
      std::vector<uint8_t> current_material) {
    std::vector<uint8_t> new_material;
    if (DerivePBKDF2KeyFromRawKey(current_material, options_.ratchet_salt, 256,
                                  &new_material) != 0) {
      return std::vector<uint8_t>();
    }
    return new_material;
  }
 protected:
  bool have_valid_key = false;
 private:
  int current_key_index_ = 0;
  KeyProviderOptions options_;
  std::vector<std::shared_ptr<KeySet>> crypto_key_ring_;
};

class KeyProvider : public rtc::RefCountInterface {
 public:
  enum { kRawKeySize = 32 };

 public:
  virtual const std::shared_ptr<ParticipantKeyHandler> GetKey(
      const std::string participant_id) const = 0;

  virtual bool SetKey(const std::string participant_id,
                      int index,
                      std::vector<uint8_t> key) = 0;

  virtual const std::vector<uint8_t> RatchetKey(
      const std::string participant_id,
      int key_index) = 0;

  virtual const std::vector<uint8_t> ExportKey(const std::string participant_id,
                                               int key_index) const = 0;

  virtual KeyProviderOptions& options() = 0;

 protected:
  virtual ~KeyProvider() {}
};

class DefaultKeyProviderImpl : public KeyProvider {
 public:
  DefaultKeyProviderImpl(KeyProviderOptions options) : options_(options) {}
  ~DefaultKeyProviderImpl() override = default;

  /// Set the key at the given index.
  bool SetKey(const std::string participant_id,
              int index,
              std::vector<uint8_t> key) override {
    webrtc::MutexLock lock(&mutex_);

    if (keys_.find(participant_id) == keys_.end()) {
      keys_[participant_id] = std::make_shared<ParticipantKeyHandler>(options_);
    }

    auto key_handler = keys_[participant_id];
    key_handler->SetKey(key, index);
    return true;
  }

  const std::shared_ptr<ParticipantKeyHandler> GetKey(
      const std::string participant_id) const override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return nullptr;
    }

    return keys_.find(participant_id)->second;
  }

  const std::vector<uint8_t> RatchetKey(const std::string participant_id,
                                        int key_index) override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<uint8_t>();
    }

    return keys_[participant_id]->RatchetKey(key_index);
  }

  const std::vector<uint8_t> ExportKey(const std::string participant_id,
                                       int key_index) const override {
    webrtc::MutexLock lock(&mutex_);
    if (keys_.find(participant_id) == keys_.end()) {
      return std::vector<uint8_t>();
    }

    auto key_set = GetKey(participant_id);

    if (!key_set) {
      return std::vector<uint8_t>();
    }

    return key_set->GetKeySet(key_index)->material;
  }

  KeyProviderOptions& options() override { return options_; }

 private:
  mutable webrtc::Mutex mutex_;
  KeyProviderOptions options_;
  std::unordered_map<std::string, std::shared_ptr<ParticipantKeyHandler>> keys_;
};

enum FrameCryptionState {
  kNew = 0,
  kOk,
  kEncryptionFailed,
  kDecryptionFailed,
  kMissingKey,
  kKeyRatcheted,
  kInternalError,
};

class FrameCryptorTransformerObserver {
 public:
  virtual void OnFrameCryptionStateChanged(const std::string participant_id,
                                           FrameCryptionState error) = 0;

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
                                   rtc::scoped_refptr<KeyProvider> key_provider);

  virtual void SetFrameCryptorTransformerObserver(
      FrameCryptorTransformerObserver* observer) {
    webrtc::MutexLock lock(&mutex_);
    observer_ = observer;
  }

  virtual void SetKeyIndex(int index) {
    webrtc::MutexLock lock(&mutex_);
    key_index_ = index;
  }

  virtual int key_index() const { return key_index_; }

  virtual void SetEnabled(bool enabled) {
    webrtc::MutexLock lock(&mutex_);
    enabled_cryption_ = enabled;
  }
  virtual bool enabled() const {
    webrtc::MutexLock lock(&mutex_);
    return enabled_cryption_;
  }
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
  std::map<uint32_t, uint32_t> send_counts_;
  rtc::scoped_refptr<KeyProvider> key_provider_;
  FrameCryptorTransformerObserver* observer_ = nullptr;
  std::unique_ptr<rtc::Thread> thread_;
  FrameCryptionState last_enc_error_ = FrameCryptionState::kNew;
  FrameCryptionState last_dec_error_ = FrameCryptionState::kNew;
};

}  // namespace webrtc

#endif  // WEBRTC_FRAME_CRYPTOR_TRANSFORMER_H_
