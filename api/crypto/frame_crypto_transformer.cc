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

#include "frame_crypto_transformer.h"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/logging.h"

enum class EncryptOrDecrypt { kEncrypt = 0, kDecrypt };

#define Success 0
#define ErrorUnexpected -1
#define OperationError -2
#define ErrorDataTooSmall -3
#define ErrorInvalidAesGcmTagLength -4

webrtc::VideoCodecType get_video_codec_type(
    webrtc::TransformableFrameInterface* frame) {
  auto videoFrame =
      static_cast<webrtc::TransformableVideoFrameInterface*>(frame);
  return videoFrame->header().codec;
}

webrtc::H264PacketizationMode get_h264_packetization_mode(
    webrtc::TransformableFrameInterface* frame) {
  auto video_frame =
      static_cast<webrtc::TransformableVideoFrameInterface*>(frame);
  const auto& h264_header = absl::get<webrtc::RTPVideoHeaderH264>(
      video_frame->header().video_type_header);
  return h264_header.packetization_mode;
}

const EVP_AEAD* GetAesGcmAlgorithmFromKeySize(size_t key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_gcm();
    case 32:
      return EVP_aead_aes_256_gcm();
    default:
      return nullptr;
  }
}

const EVP_CIPHER* GetAesCbcAlgorithmFromKeySize(size_t key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aes_128_cbc();
    case 32:
      return EVP_aes_256_cbc();
    default:
      return nullptr;
  }
}

std::string to_uint8_list(const uint8_t* data, int len) {
  std::stringstream ss;
  ss << "[";
  for (int i = 0; i < len; i++) {
    ss << static_cast<unsigned>(data[i]) << ",";
  }
  ss << "]";
  return ss.str();
}

std::string to_hex(const uint8_t* data, int len) {
  std::stringstream ss;
  ss << std::uppercase << std::hex << std::setfill('0');
  for (int i = 0; i < len; i++) {
    ss << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  return ss.str();
}

uint8_t get_unencrypted_bytes(webrtc::TransformableFrameInterface* frame,
                              webrtc::FrameCryptorTransformer::MediaType type) {
  uint8_t unencrypted_bytes = 0;
  switch (type) {
    case webrtc::FrameCryptorTransformer::MediaType::kAudioFrame:
      unencrypted_bytes = 1;
      break;
    case webrtc::FrameCryptorTransformer::MediaType::kVideoFrame: {
      auto videoFrame =
          static_cast<webrtc::TransformableVideoFrameInterface*>(frame);
      if (videoFrame->header().codec ==
          webrtc::VideoCodecType::kVideoCodecAV1) {
        unencrypted_bytes = 0;
      } else if (videoFrame->header().codec ==
                 webrtc::VideoCodecType::kVideoCodecVP8) {
        unencrypted_bytes = videoFrame->IsKeyFrame() ? 10 : 3;
      } else if (videoFrame->header().codec ==
                 webrtc::VideoCodecType::kVideoCodecH264) {
        rtc::ArrayView<const uint8_t> date_in = frame->GetData();
        std::vector<webrtc::H264::NaluIndex> nalu_indices =
            webrtc::H264::FindNaluIndices(date_in.data(), date_in.size());

        int idx = 0;
        for (const auto& index : nalu_indices) {
          const uint8_t* slice = date_in.data() + index.payload_start_offset;
          webrtc::H264::NaluType nalu_type =
              webrtc::H264::ParseNaluType(slice[0]);
          switch (nalu_type) {
            case webrtc::H264::NaluType::kIdr:
            case webrtc::H264::NaluType::kSlice:
              unencrypted_bytes = index.payload_start_offset + 2;
              RTC_LOG(LS_INFO)
                  << "NonParameterSetNalu::payload_size: " << index.payload_size
                  << ", nalu_type " << nalu_type << ", NaluIndex [" << idx++
                  << "] offset: " << index.payload_start_offset;
              return unencrypted_bytes;
            default:
              break;
          }
        }
      }
      break;
    }
    default:
      break;
  }
  return unencrypted_bytes;
}

int DerivePBKDF2KeyFromRawKey(const std::vector<uint8_t> raw_key,
                              const std::vector<uint8_t>& salt,
                              unsigned int optional_length_bits,
                              std::vector<uint8_t>* derived_key) {
  size_t key_size_bytes = optional_length_bits / 8;
  derived_key->resize(key_size_bytes);

  if (PKCS5_PBKDF2_HMAC((const char*)raw_key.data(), raw_key.size(),
                        salt.data(), salt.size(), 100000, EVP_sha256(),
                        key_size_bytes, derived_key->data()) != 1) {
    RTC_LOG(LS_ERROR) << "Failed to derive AES key from password.";
    return ErrorUnexpected;
  }

  RTC_LOG(LS_INFO) << "raw_key "
                   << to_uint8_list(raw_key.data(), raw_key.size()) << " len "
                   << raw_key.size() << " slat << "
                   << to_uint8_list(salt.data(), salt.size()) << " len "
                   << salt.size() << "\n derived_key "
                   << to_uint8_list(derived_key->data(), derived_key->size())
                   << " len " << derived_key->size();

  return Success;
}

int AesGcmEncryptDecrypt(EncryptOrDecrypt mode,
                         const std::vector<uint8_t> raw_key,
                         const rtc::ArrayView<uint8_t> data,
                         unsigned int tag_length_bytes,
                         rtc::ArrayView<uint8_t> iv,
                         rtc::ArrayView<uint8_t> additional_data,
                         const EVP_AEAD* aead_alg,
                         std::vector<uint8_t>* buffer) {
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!aead_alg) {
    RTC_LOG(LS_ERROR) << "Invalid AES-GCM key size.";
    return ErrorUnexpected;
  }

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_alg, raw_key.data(), raw_key.size(),
                         tag_length_bytes, nullptr)) {
    RTC_LOG(LS_ERROR) << "Failed to initialize AES-GCM context.";
    return OperationError;
  }

  size_t len;
  int ok;

  if (mode == EncryptOrDecrypt::kDecrypt) {
    if (data.size() < tag_length_bytes) {
      RTC_LOG(LS_ERROR) << "Data too small for AES-GCM tag.";
      return ErrorDataTooSmall;
    }

    buffer->resize(data.size() - tag_length_bytes);

    ok = EVP_AEAD_CTX_open(ctx.get(), buffer->data(), &len, buffer->size(),
                           iv.data(), iv.size(), data.data(), data.size(),
                           additional_data.data(), additional_data.size());
  } else {
    buffer->resize(data.size() + EVP_AEAD_max_overhead(aead_alg));

    ok = EVP_AEAD_CTX_seal(ctx.get(), buffer->data(), &len, buffer->size(),
                           iv.data(), iv.size(), data.data(), data.size(),
                           additional_data.data(), additional_data.size());
  }

  if (!ok) {
    RTC_LOG(LS_WARNING) << "Failed to perform AES-GCM operation.";
    return OperationError;
  }

  buffer->resize(len);

  return Success;
}

int AesCbcEncryptDecrypt(EncryptOrDecrypt mode,
                         const std::vector<uint8_t>& raw_key,
                         rtc::ArrayView<uint8_t> iv,
                         const rtc::ArrayView<uint8_t> input,
                         std::vector<uint8_t>* output) {
  const EVP_CIPHER* cipher = GetAesCbcAlgorithmFromKeySize(raw_key.size());
  RTC_DCHECK(cipher);  // Already handled in Init();
  RTC_DCHECK_EQ(EVP_CIPHER_iv_length(cipher), iv.size());
  RTC_DCHECK_EQ(EVP_CIPHER_key_length(cipher), raw_key.size());

  bssl::ScopedEVP_CIPHER_CTX ctx;
  if (!EVP_CipherInit_ex(ctx.get(), cipher, nullptr,
                         reinterpret_cast<const uint8_t*>(raw_key.data()),
                         iv.data(),
                         mode == EncryptOrDecrypt::kEncrypt ? 1 : 0)) {
    return OperationError;
  }

  // Encrypting needs a block size of space to allow for any padding.
  output->resize(input.size() +
                 (mode == EncryptOrDecrypt::kEncrypt ? iv.size() : 0));
  int out_len;
  if (!EVP_CipherUpdate(ctx.get(), output->data(), &out_len, input.data(),
                        input.size()))
    return OperationError;

  // Write out the final block plus padding (if any) to the end of the data
  // just written.
  int tail_len;
  if (!EVP_CipherFinal_ex(ctx.get(), output->data() + out_len, &tail_len))
    return OperationError;

  out_len += tail_len;
  RTC_CHECK_LE(out_len, static_cast<int>(output->size()));
  return Success;
}

int AesEncryptDecrypt(EncryptOrDecrypt mode,
                      webrtc::FrameCryptorTransformer::Algorithm algorithm,
                      const std::vector<uint8_t>& raw_key,
                      rtc::ArrayView<uint8_t> iv,
                      rtc::ArrayView<uint8_t> additional_data,
                      const rtc::ArrayView<uint8_t> data,
                      std::vector<uint8_t>* buffer) {
  switch (algorithm) {
    case webrtc::FrameCryptorTransformer::Algorithm::kAesGcm: {
      unsigned int tag_length_bits = 128;
      return AesGcmEncryptDecrypt(
          mode, raw_key, data, tag_length_bits / 8, iv, additional_data,
          GetAesGcmAlgorithmFromKeySize(raw_key.size()), buffer);
    }
    case webrtc::FrameCryptorTransformer::Algorithm::kAesCbc:
      return AesCbcEncryptDecrypt(mode, raw_key, iv, data, buffer);
  }
}

namespace webrtc {

FrameCryptorTransformer::FrameCryptorTransformer(
    const std::string participant_id,
    MediaType type,
    Algorithm algorithm,
    rtc::scoped_refptr<KeyProvider> key_provider)
    : participant_id_(participant_id),
      type_(type),
      algorithm_(algorithm),
      key_provider_(key_provider) {
  RTC_DCHECK(key_provider_ != nullptr);
}

void FrameCryptorTransformer::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  webrtc::MutexLock lock(&sink_mutex_);
  if (sink_callback_ == nullptr && sink_callbacks_.size() == 0) {
    RTC_LOG(LS_WARNING)
        << "FrameCryptorTransformer::Transform sink_callback_ is NULL";
    return;
  }

  // do encrypt or decrypt here...
  switch (frame->GetDirection()) {
    case webrtc::TransformableFrameInterface::Direction::kSender:
      encryptFrame(std::move(frame));
      break;
    case webrtc::TransformableFrameInterface::Direction::kReceiver:
      decryptFrame(std::move(frame));
      break;
    case webrtc::TransformableFrameInterface::Direction::kUnknown:
      // do nothing
      RTC_LOG(LS_INFO) << "FrameCryptorTransformer::Transform() kUnknown";
      break;
  }
}

void FrameCryptorTransformer::encryptFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  bool enabled_cryption = false;
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink_callback = nullptr;
  {
    webrtc::MutexLock lock(&mutex_);
    enabled_cryption = enabled_cryption_;
    if (type_ == webrtc::FrameCryptorTransformer::MediaType::kAudioFrame) {
      sink_callback = sink_callback_;
    } else {
      sink_callback = sink_callbacks_[frame->GetSsrc()];
    }
  }

  if (sink_callback == nullptr) {
    RTC_LOG(LS_WARNING)
        << "FrameCryptorTransformer::encryptFrame() sink_callback is NULL";
    if (last_enc_error_ != FrameCryptionState::kInternalError) {
      last_enc_error_ = FrameCryptionState::kInternalError;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_enc_error_);
    }
    return;
  }

  rtc::ArrayView<const uint8_t> date_in = frame->GetData();
  if (date_in.size() == 0 || !enabled_cryption) {
    sink_callback->OnTransformedFrame(std::move(frame));
    return;
  }

  auto key_handler = key_provider_->options().shared_key
                         ? key_provider_->GetSharedKey(participant_id_)
                         : key_provider_->GetKey(participant_id_);

  if (key_handler == nullptr || key_handler->GetKeySet(key_index_) == nullptr) {
    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::encryptFrame() no keys, or "
                        "key_index["
                     << key_index_ << "] out of range for participant "
                     << participant_id_;
    if (last_enc_error_ != FrameCryptionState::kMissingKey) {
      last_enc_error_ = FrameCryptionState::kMissingKey;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_enc_error_);
    }
    return;
  }

  auto key_set = key_handler->GetKeySet(key_index_);
  uint8_t unencrypted_bytes = get_unencrypted_bytes(frame.get(), type_);

  rtc::Buffer frame_header(unencrypted_bytes);
  for (size_t i = 0; i < unencrypted_bytes; i++) {
    frame_header[i] = date_in[i];
  }

  rtc::Buffer frame_trailer(2);
  frame_trailer[0] = getIvSize();
  frame_trailer[1] = key_index_;
  rtc::Buffer iv = makeIv(frame->GetSsrc(), frame->GetTimestamp());

  rtc::Buffer payload(date_in.size() - unencrypted_bytes);
  for (size_t i = unencrypted_bytes; i < date_in.size(); i++) {
    payload[i - unencrypted_bytes] = date_in[i];
  }

  std::vector<uint8_t> buffer;
  if (AesEncryptDecrypt(EncryptOrDecrypt::kEncrypt, algorithm_,
                        key_set->encryption_key, iv, frame_header, payload,
                        &buffer) == Success) {
    rtc::Buffer encrypted_payload(buffer.data(), buffer.size());
    rtc::Buffer data_out;
    data_out.AppendData(frame_header);
    data_out.AppendData(encrypted_payload);
    data_out.AppendData(iv);
    data_out.AppendData(frame_trailer);

    RTC_CHECK_EQ(data_out.size(), frame_header.size() +
                                      encrypted_payload.size() + iv.size() +
                                      frame_trailer.size());

    frame->SetData(data_out);

    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::encryptFrame() ivLength="
                     << static_cast<int>(iv.size()) << " unencrypted_bytes="
                     << static_cast<int>(unencrypted_bytes)
                     << " key_index=" << static_cast<int>(key_index_)
                     << " aesKey="
                     << to_hex(key_set->encryption_key.data(),
                               key_set->encryption_key.size())
                     << " iv=" << to_hex(iv.data(), iv.size());
    if (last_enc_error_ != FrameCryptionState::kOk) {
      last_enc_error_ = FrameCryptionState::kOk;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_enc_error_);
    }
    sink_callback->OnTransformedFrame(std::move(frame));
  } else {
    if (last_enc_error_ != FrameCryptionState::kEncryptionFailed) {
      last_enc_error_ = FrameCryptionState::kEncryptionFailed;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_enc_error_);
    }
    RTC_LOG(LS_ERROR) << "FrameCryptorTransformer::encryptFrame() failed";
  }
}

void FrameCryptorTransformer::decryptFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  bool enabled_cryption = false;
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink_callback = nullptr;
  {
    webrtc::MutexLock lock(&mutex_);
    enabled_cryption = enabled_cryption_;
    if (type_ == webrtc::FrameCryptorTransformer::MediaType::kAudioFrame) {
      sink_callback = sink_callback_;
    } else {
      sink_callback = sink_callbacks_[frame->GetSsrc()];
    }
  }

  if (sink_callback == nullptr) {
    RTC_LOG(LS_WARNING)
        << "FrameCryptorTransformer::decryptFrame() sink_callback is NULL";
    if (last_dec_error_ != FrameCryptionState::kInternalError) {
      last_dec_error_ = FrameCryptionState::kInternalError;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_dec_error_);
    }
    return;
  }

  rtc::ArrayView<const uint8_t> date_in = frame->GetData();

  if (date_in.size() == 0 || !enabled_cryption) {
    sink_callback->OnTransformedFrame(std::move(frame));
    return;
  }

  auto uncrypted_magic_bytes = key_provider_->options().uncrypted_magic_bytes;
  if (uncrypted_magic_bytes.size() > 0 &&
      date_in.size() >= uncrypted_magic_bytes.size() + 1) {
    auto tmp =
        date_in.subview(date_in.size() - (uncrypted_magic_bytes.size() + 1),
                        uncrypted_magic_bytes.size());

    if (uncrypted_magic_bytes == std::vector<uint8_t>(tmp.begin(), tmp.end())) {
      RTC_CHECK_EQ(tmp.size(), uncrypted_magic_bytes.size());
      auto frame_type = date_in.subview(date_in.size() - 1, 1);
      RTC_CHECK_EQ(frame_type.size(), 1);

      RTC_LOG(LS_INFO)
          << "FrameCryptorTransformer::uncrypted_magic_bytes( type "
          << frame_type[0] << ", tmp " << to_hex(tmp.data(), tmp.size())
          << ", magic bytes "
          << to_hex(uncrypted_magic_bytes.data(), uncrypted_magic_bytes.size())
          << ")";

      // magic bytes detected, this is a non-encrypted frame, skip frame
      // decryption.
      rtc::Buffer data_out;
      data_out.AppendData(date_in.subview(
          0, date_in.size() - uncrypted_magic_bytes.size() - 1));
      frame->SetData(data_out);
      sink_callback->OnTransformedFrame(std::move(frame));
      return;
    }
  }

  uint8_t unencrypted_bytes = get_unencrypted_bytes(frame.get(), type_);

  rtc::Buffer frame_header(unencrypted_bytes);
  for (size_t i = 0; i < unencrypted_bytes; i++) {
    frame_header[i] = date_in[i];
  }

  rtc::Buffer frame_trailer(2);
  frame_trailer[0] = date_in[date_in.size() - 2];
  frame_trailer[1] = date_in[date_in.size() - 1];
  uint8_t ivLength = frame_trailer[0];
  uint8_t key_index = frame_trailer[1];

  if (ivLength != getIvSize()) {
    RTC_LOG(LS_WARNING) << "FrameCryptorTransformer::decryptFrame() ivLength["
                      << static_cast<int>(ivLength) << "] != getIvSize()["
                      << static_cast<int>(getIvSize()) << "]";
    if (last_dec_error_ != FrameCryptionState::kDecryptionFailed) {
      last_dec_error_ = FrameCryptionState::kDecryptionFailed;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_dec_error_);
    }
    return;
  }

  auto key_handler = key_provider_->options().shared_key
                         ? key_provider_->GetSharedKey(participant_id_)
                         : key_provider_->GetKey(participant_id_);

  if (key_index >= KEYRING_SIZE || key_handler == nullptr ||
      key_handler->GetKeySet(key_index) == nullptr) {
    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::decryptFrame() no keys, or "
                        "key_index["
                     << key_index_ << "] out of range for participant "
                     << participant_id_;
    if (last_dec_error_ != FrameCryptionState::kMissingKey) {
      last_dec_error_ = FrameCryptionState::kMissingKey;
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_dec_error_);
    }
    return;
  }

  if (last_dec_error_ == kDecryptionFailed && !key_handler->HasValidKey()) {
    // if decryption failed and we have an invalid key,
    // please try to decrypt with the next new key
    return;
  }

  auto key_set = key_handler->GetKeySet(key_index);

  rtc::Buffer iv = rtc::Buffer(ivLength);
  for (size_t i = 0; i < ivLength; i++) {
    iv[i] = date_in[date_in.size() - 2 - ivLength + i];
  }

  rtc::Buffer encrypted_payload(date_in.size() - unencrypted_bytes - ivLength -
                                2);
  for (size_t i = unencrypted_bytes; i < date_in.size() - ivLength - 2; i++) {
    encrypted_payload[i - unencrypted_bytes] = date_in[i];
  }
  std::vector<uint8_t> buffer;

  int ratchet_count = 0;
  auto initialKeyMaterial = key_set->material;
  bool decryption_success = false;
  if (AesEncryptDecrypt(EncryptOrDecrypt::kDecrypt, algorithm_,
                        key_set->encryption_key, iv, frame_header,
                        encrypted_payload, &buffer) == Success) {
    decryption_success = true;
  } else {
    RTC_LOG(LS_WARNING) << "FrameCryptorTransformer::decryptFrame() failed";
    std::shared_ptr<ParticipantKeyHandler::KeySet> ratcheted_key_set;
    auto currentKeyMaterial = key_set->material;
    if (key_provider_->options().ratchet_window_size > 0) {
      while (ratchet_count < key_provider_->options().ratchet_window_size) {
        ratchet_count++;

        RTC_LOG(LS_INFO) << "ratcheting key attempt " << ratchet_count << " of "
                         << key_provider_->options().ratchet_window_size;

        auto new_material = key_handler->RatchetKeyMaterial(currentKeyMaterial);
        ratcheted_key_set = key_handler->DeriveKeys(
            new_material, key_provider_->options().ratchet_salt, 128);

        if (AesEncryptDecrypt(EncryptOrDecrypt::kDecrypt, algorithm_,
                              ratcheted_key_set->encryption_key, iv,
                              frame_header, encrypted_payload,
                              &buffer) == Success) {
          RTC_LOG(LS_INFO) << "FrameCryptorTransformer::decryptFrame() "
                              "ratcheted to key_index="
                           << static_cast<int>(key_index);
          decryption_success = true;
          // success, so we set the new key
          key_handler->SetKeyFromMaterial(new_material, key_index);
          key_handler->SetHasValidKey();
          if (last_dec_error_ != FrameCryptionState::kKeyRatcheted) {
            last_dec_error_ = FrameCryptionState::kKeyRatcheted;
            if (observer_)
              observer_->OnFrameCryptionStateChanged(participant_id_,
                                                     last_dec_error_);
          }
          break;
        }
        // for the next ratchet attempt
        currentKeyMaterial = new_material;
      }

      /* Since the key it is first send and only afterwards actually used for
        encrypting, there were situations when the decrypting failed due to the
        fact that the received frame was not encrypted yet and ratcheting, of
        course, did not solve the problem. So if we fail RATCHET_WINDOW_SIZE
        times, we come back to the initial key.
       */
      if (!decryption_success ||
          ratchet_count >= key_provider_->options().ratchet_window_size) {
        key_handler->SetKeyFromMaterial(initialKeyMaterial, key_index);
      }
    }
  }

  if (!decryption_success) {
    if (last_dec_error_ != FrameCryptionState::kDecryptionFailed) {
      last_dec_error_ = FrameCryptionState::kDecryptionFailed;
      key_handler->DecryptionFailure();
      if (observer_)
        observer_->OnFrameCryptionStateChanged(participant_id_,
                                               last_dec_error_);
    }
    return;
  }

  rtc::Buffer payload(buffer.data(), buffer.size());
  rtc::Buffer data_out;
  data_out.AppendData(frame_header);
  data_out.AppendData(payload);
  frame->SetData(data_out);

  RTC_LOG(LS_INFO) << "FrameCryptorTransformer::decryptFrame() ivLength="
                   << static_cast<int>(ivLength) << " unencrypted_bytes="
                   << static_cast<int>(unencrypted_bytes)
                   << " key_index=" << static_cast<int>(key_index_)
                   << " aesKey="
                   << to_hex(key_set->encryption_key.data(),
                             key_set->encryption_key.size())
                   << " iv=" << to_hex(iv.data(), iv.size());

  if (last_dec_error_ != FrameCryptionState::kOk) {
    last_dec_error_ = FrameCryptionState::kOk;
    if (observer_)
      observer_->OnFrameCryptionStateChanged(participant_id_, last_dec_error_);
  }
  sink_callback->OnTransformedFrame(std::move(frame));
}

rtc::Buffer FrameCryptorTransformer::makeIv(uint32_t ssrc, uint32_t timestamp) {
  uint32_t send_count = 0;
  if (send_counts_.find(ssrc) == send_counts_.end()) {
    srand((unsigned)time(NULL));
    send_counts_[ssrc] = floor(rand() * 0xFFFF);
  } else {
    send_count = send_counts_[ssrc];
  }
  rtc::ByteBufferWriter buf;
  buf.WriteUInt32(ssrc);
  buf.WriteUInt32(timestamp);
  buf.WriteUInt32(timestamp - (send_count % 0xFFFF));
  send_counts_[ssrc] = send_count + 1;

  RTC_CHECK_EQ(buf.Length(), getIvSize());

  return rtc::Buffer(buf.Data(), buf.Length());
}

uint8_t FrameCryptorTransformer::getIvSize() {
  switch (algorithm_) {
    case Algorithm::kAesGcm:
      return 12;
    case Algorithm::kAesCbc:
      return 16;
    default:
      return 0;
  }
}

}  // namespace webrtc
