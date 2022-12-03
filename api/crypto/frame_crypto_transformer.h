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

namespace webrtc {

class KeyManager {
 public:
  enum { kMaxKeySize = 32 };

 public:
  virtual const std::vector<std::vector<uint8_t>> keys() const = 0;

 protected:
  virtual ~KeyManager() {}
};

class KeyManagerImpl : public KeyManager {
 public:
  virtual bool SetKey(int index, std::vector<uint8_t> key) {
    if (index > kMaxKeySize) {
      return false;
    }
    webrtc::MutexLock lock(&mutex_);
    if (index > keys_.size()) {
      keys_.resize(index + 1);
    }
    keys_[index] = key;
    return true;
  }

  virtual bool SetKeys(std::vector<std::vector<uint8_t>> keys) {
    webrtc::MutexLock lock(&mutex_);
    keys_ = keys;
    return true;
  }

  virtual bool AddKey(std::vector<uint8_t> key) {
    webrtc::MutexLock lock(&mutex_);
    keys_.push_back(key);
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

class FrameCryptorTransformer
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

  explicit FrameCryptorTransformer(MediaType type,
                                   Algorithm algorithm = Algorithm::kAesGcm,
                                   std::shared_ptr<KeyManager> key_manager);

  virtual void SetKeyIndex(int index);
  virtual void SetEnabled(bool enable);

 protected:
  virtual void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>) override;
  virtual void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>,
      uint32_t ssrc) override;
  virtual void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;
  virtual void UnregisterTransformedFrameCallback() override;
  virtual void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override;

 private:
  void encryptFrame(std::unique_ptr<webrtc::TransformableFrameInterface> frame);
  void decryptFrame(std::unique_ptr<webrtc::TransformableFrameInterface> frame);
  rtc::Buffer makeIv(uint32_t ssrc, uint32_t timestamp);

 private:
  mutable webrtc::Mutex mutex_;
  mutable webrtc::Mutex sink_mutex_;
  bool enabled_cryption_ RTC_GUARDED_BY(mutex_) = false;
  MediaType type_;
  Algorithm algorithm_;
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink_callback_;

  int key_index_ RTC_GUARDED_BY(mutex_) = 0;
  std::map<uint32_t, uint32_t> sendCounts_;
  std::shared_ptr<KeyManager> key_manager_;
};

}  // namespace webrtc

#endif  // WEBRTC_FRAME_CRYPTOR_TRANSFORMER_H_