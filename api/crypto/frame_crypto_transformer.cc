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
            case webrtc::H264::NaluType::kSps:
            case webrtc::H264::NaluType::kPps:
            case webrtc::H264::NaluType::kAud:
            case webrtc::H264::NaluType::kSei:
            case webrtc::H264::NaluType::kPrefix:
              RTC_LOG(LS_INFO)
                  << "ParameterSetNalu payload_size: " << index.payload_size << ", nalu_type "
                  << nalu_type << ", NaluIndex [" << idx++
                  << "] offset: " << index.payload_start_offset;
              break;  // Ignore these nalus, as we don't care about their
                      // contents.
            default:
              RTC_LOG(LS_INFO)
                  << "NonParameterSetNalu payload_size: " << index.payload_size << ", nalu_type "
                  << nalu_type << ", NaluIndex [" << idx++
                  << "] offset: " << index.payload_start_offset;
              unencrypted_bytes = index.payload_start_offset + 1;
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
    RTC_LOG(LS_ERROR) << "Failed to perform AES-GCM operation.";
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
    rtc::scoped_refptr<KeyManager> key_manager)
    : participant_id_(participant_id),
      type_(type),
      algorithm_(algorithm),
      key_manager_(key_manager) {}

void FrameCryptorTransformer::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  webrtc::MutexLock lock(&sink_mutex_);
  if (sink_callback_ == nullptr) {
    RTC_LOG(LS_WARNING)
        << "FrameCryptorTransformer::Transform sink_callback_ is NULL";
    return;
  }

  bool enabled_cryption = false;
  {
    webrtc::MutexLock lock(&mutex_);
    enabled_cryption = enabled_cryption_;
  }

  if ((frame->GetData().size() == 0 && sink_callback_) || !key_manager_ ||
      !enabled_cryption) {
    sink_callback_->OnTransformedFrame(std::move(frame));
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
      if (sink_callback_)
        sink_callback_->OnTransformedFrame(std::move(frame));
      break;
  }
}

void ParseSlice(const uint8_t* slice, size_t length, bool enc) {
  H264::NaluType nalu_type = H264::ParseNaluType(slice[0]);
  switch (nalu_type) {
    case H264::NaluType::kSps:
    case H264::NaluType::kPps:
    case H264::NaluType::kAud:
    case H264::NaluType::kSei:
    case H264::NaluType::kPrefix:
      RTC_LOG(LS_INFO) << (enc ? "encrypto" : "decrypto")
                       << ": ParameterSetNalu length: " << length
                       << ", nalu_type " << nalu_type;
      break;  // Ignore these nalus, as we don't care about their contents.
    default:
      RTC_LOG(LS_INFO) << (enc ? "encrypto" : "decrypto")
                       << ": NonParameterSetNalu length: " << length
                       << ", nalu_type " << nalu_type;
      for (size_t i = 1; i < length; i++) {
        ((uint8_t*)slice)[i] = slice[i] ^ 0x05;
      }
      break;
  }
}

void FrameCryptorTransformer::encryptFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  auto keys = key_manager_->keys(participant_id_);

  if (keys.size() == 0 || key_index_ >= (int)keys.size()) {
    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::encryptFrame() no keys, or "
                        "key_index_ out of range";
    if (sink_callback_)
      sink_callback_->OnTransformedFrame(std::move(frame));
    return;
  }
  std::vector<uint8_t> aes_key = keys[key_index_];

  uint8_t unencrypted_bytes = get_unencrypted_bytes(frame.get(), type_);
  rtc::ArrayView<const uint8_t> date_in = frame->GetData();

  rtc::Buffer frameHeader(unencrypted_bytes);
  for (size_t i = 0; i < unencrypted_bytes; i++) {
    frameHeader[i] = date_in[i];
  }

  rtc::Buffer frameTrailer(2);
  frameTrailer[0] = getIvSize();
  frameTrailer[1] = key_index_;
  rtc::Buffer iv = makeIv(frame->GetSsrc(), frame->GetTimestamp());

  rtc::Buffer payload(date_in.size() - unencrypted_bytes);
  for (size_t i = unencrypted_bytes; i < date_in.size(); i++) {
    payload[i - unencrypted_bytes] = date_in[i];
  }

  std::vector<uint8_t> buffer;
  if (AesEncryptDecrypt(EncryptOrDecrypt::kEncrypt, algorithm_, aes_key, iv,
                        frameHeader, payload, &buffer) == Success) {
    rtc::Buffer encrypted_payload(buffer.data(), buffer.size());
    rtc::Buffer data_out;
    data_out.AppendData(frameHeader);
    data_out.AppendData(encrypted_payload);
    data_out.AppendData(iv);
    data_out.AppendData(frameTrailer);

    RTC_CHECK_EQ(data_out.size(), frameHeader.size() +
                                      encrypted_payload.size() + iv.size() +
                                      frameTrailer.size());

    frame->SetData(data_out);

    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::encryptFrame() ivLength="
                     << static_cast<int>(iv.size()) << " unencrypted_bytes="
                     << static_cast<int>(unencrypted_bytes)
                     << " keyIndex=" << static_cast<int>(key_index_)
                     << " aesKey=" << to_hex(aes_key.data(), aes_key.size())
                     << " iv=" << to_hex(iv.data(), iv.size());
  }

  if (sink_callback_)
    sink_callback_->OnTransformedFrame(std::move(frame));
}

void FrameCryptorTransformer::decryptFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  uint8_t unencrypted_bytes = get_unencrypted_bytes(frame.get(), type_);
  rtc::ArrayView<const uint8_t> date_in = frame->GetData();

  rtc::Buffer frameHeader(unencrypted_bytes);
  for (size_t i = 0; i < unencrypted_bytes; i++) {
    frameHeader[i] = date_in[i];
  }

  rtc::Buffer frameTrailer(2);
  frameTrailer[0] = date_in[date_in.size() - 2];
  frameTrailer[1] = date_in[date_in.size() - 1];
  uint8_t ivLength = frameTrailer[0];
  uint8_t key_index = frameTrailer[1];

  auto keys = key_manager_->keys(participant_id_);

  if (keys.size() == 0 || key_index >= (int)keys.size() ||
      ivLength != getIvSize()) {
    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::decryptFrame() no keys, or "
                        "key_index out of range";
    if (sink_callback_)
      sink_callback_->OnTransformedFrame(std::move(frame));
    return;
  }
  std::vector<uint8_t> aes_key = keys[key_index];

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
  if (AesEncryptDecrypt(EncryptOrDecrypt::kDecrypt, algorithm_, aes_key, iv,
                        frameHeader, encrypted_payload, &buffer) == Success) {
    rtc::Buffer payload(buffer.data(), buffer.size());
    rtc::Buffer data_out;
    data_out.AppendData(frameHeader);
    data_out.AppendData(payload);
    frame->SetData(data_out);

    RTC_LOG(LS_INFO) << "FrameCryptorTransformer::decryptFrame() ivLength="
                     << static_cast<int>(ivLength) << " unencrypted_bytes="
                     << static_cast<int>(unencrypted_bytes)
                     << " keyIndex=" << static_cast<int>(key_index_)
                     << " aesKey=" << to_hex(aes_key.data(), aes_key.size())
                     << " iv=" << to_hex(iv.data(), iv.size());
  }
  if (sink_callback_)
    sink_callback_->OnTransformedFrame(std::move(frame));
}

rtc::Buffer FrameCryptorTransformer::makeIv(uint32_t ssrc, uint32_t timestamp) {
  uint32_t sendCount = 0;
  if (sendCounts_.find(ssrc) == sendCounts_.end()) {
    srand((unsigned)time(NULL));
    sendCounts_[ssrc] = floor(rand() * 0xFFFF);
  } else {
    sendCount = sendCounts_[ssrc];
  }
  rtc::ByteBufferWriter buf;
  buf.WriteUInt32(ssrc);
  buf.WriteUInt32(timestamp);
  buf.WriteUInt32(sendCount % 0xFFFF);
  sendCounts_[ssrc] = sendCount + 1;

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
