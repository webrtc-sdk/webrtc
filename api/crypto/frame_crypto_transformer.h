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

#include "api/frame_transformer_interface.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace webrtc {

class KeyManager : public rtc::RefCountInterface {
 public:
  enum { kRawKeySize = 32 };

 public:
  virtual const std::vector<std::vector<uint8_t>> keys(
      const std::string participant_id) const = 0;

 protected:
  virtual ~KeyManager() {}
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
