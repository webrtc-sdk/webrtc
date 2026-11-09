/*
 * Copyright 2026 LiveKit, Inc.
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

#include "api/frame_metadata/frame_metadata_transformer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace frame_metadata {
namespace {

constexpr uint8_t kXorMask = 0xFF;

constexpr uint8_t kAv1ObuForbiddenBit = 0b1000'0000;
constexpr uint8_t kAv1ObuTypeMask = 0b0111'1000;
constexpr uint8_t kAv1ObuExtensionFlag = 0b0000'0100;
constexpr uint8_t kAv1ObuSizePresentBit = 0b0000'0010;
constexpr uint8_t kAv1ObuTypeSequenceHeader = 1;
constexpr uint8_t kAv1ObuTypeTemporalDelimiter = 2;
constexpr uint8_t kAv1ObuTypeMetadata = 5;

void AppendXored(uint8_t value, std::vector<uint8_t>& out) {
  out.push_back(static_cast<uint8_t>(value ^ kXorMask));
}

void AppendTlvHeader(uint8_t tag, size_t length, std::vector<uint8_t>& out) {
  AppendXored(tag, out);
  AppendXored(static_cast<uint8_t>(length), out);
}

// Writes the low `size` bytes of `value`, most significant first.
void AppendXoredBigEndian(uint64_t value,
                          size_t size,
                          std::vector<uint8_t>& out) {
  for (size_t i = size; i > 0; --i) {
    AppendXored(static_cast<uint8_t>(value >> ((i - 1) * 8)), out);
  }
}

uint64_t ReadXoredBigEndian(std::span<const uint8_t> bytes) {
  uint64_t value = 0;
  for (uint8_t byte : bytes) {
    value = (value << 8) | static_cast<uint8_t>(byte ^ kXorMask);
  }
  return value;
}

// Returns an empty vector when no field is set.
std::vector<uint8_t> EncodeTrailer(uint64_t user_timestamp_us,
                                   uint32_t frame_id,
                                   std::span<const uint8_t> user_data) {
  const bool has_timestamp = user_timestamp_us != 0;
  const bool has_frame_id = frame_id != 0;
  const bool has_user_data = !user_data.empty();
  if (!has_timestamp && !has_frame_id && !has_user_data) {
    return {};
  }

  const size_t trailer_size =
      (has_timestamp ? kTimestampTlvSize : 0) +
      (has_frame_id ? kFrameIdTlvSize : 0) +
      (has_user_data ? kTlvHeaderSize + user_data.size() : 0) + kEnvelopeSize;
  RTC_DCHECK_LE(trailer_size, kMaxTrailerSize);

  std::vector<uint8_t> trailer;
  trailer.reserve(trailer_size);
  if (has_timestamp) {
    AppendTlvHeader(kTagTimestampUs, sizeof(user_timestamp_us), trailer);
    AppendXoredBigEndian(user_timestamp_us, sizeof(user_timestamp_us), trailer);
  }
  if (has_frame_id) {
    AppendTlvHeader(kTagFrameId, sizeof(frame_id), trailer);
    AppendXoredBigEndian(frame_id, sizeof(frame_id), trailer);
  }
  if (has_user_data) {
    AppendTlvHeader(kTagUserData, user_data.size(), trailer);
    for (uint8_t byte : user_data) {
      AppendXored(byte, trailer);
    }
  }
  AppendXored(static_cast<uint8_t>(trailer_size), trailer);
  trailer.insert(trailer.end(), std::begin(kMagic), std::end(kMagic));
  return trailer;
}

// `bytes` must be at least as long as the magic.
bool EndsWithMagic(std::span<const uint8_t> bytes) {
  return std::ranges::equal(bytes.last(sizeof(kMagic)), kMagic);
}

bool ContainsStartCode(std::span<const uint8_t> bytes) {
  for (size_t i = 0; i + 2 < bytes.size(); ++i) {
    if (bytes[i] == 0 && bytes[i + 1] == 0 && bytes[i + 2] == 1) {
      return true;
    }
  }
  return false;
}

void WriteLeb128(uint64_t value, std::vector<uint8_t>& out) {
  while (value >= 0x80) {
    out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<uint8_t>(value));
}

bool ReadLeb128(std::span<const uint8_t> data, size_t& pos, uint64_t& value) {
  value = 0;
  int shift = 0;
  for (int bytes = 0; bytes < 8; ++bytes) {
    if (pos >= data.size()) {
      return false;
    }
    const uint8_t byte = data[pos++];
    value |= static_cast<uint64_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      return true;
    }
    shift += 7;
  }
  return false;
}

struct Av1Obu {
  uint8_t type = 0;
  size_t start = 0;
  size_t payload_start = 0;
  size_t end = 0;
};

// Reads the OBU starting at `pos`. Returns nullopt if it is malformed.
std::optional<Av1Obu> ReadAv1Obu(std::span<const uint8_t> data, size_t pos) {
  Av1Obu obu;
  obu.start = pos;
  const uint8_t header = data[pos++];
  if ((header & kAv1ObuForbiddenBit) != 0) {
    return std::nullopt;
  }
  obu.type = static_cast<uint8_t>((header & kAv1ObuTypeMask) >> 3);

  if ((header & kAv1ObuExtensionFlag) != 0) {
    if (pos >= data.size()) {
      return std::nullopt;
    }
    ++pos;
  }

  // An OBU without a size field runs to the end of the data.
  size_t payload_size = data.size() - pos;
  if ((header & kAv1ObuSizePresentBit) != 0) {
    uint64_t leb_payload_size = 0;
    if (!ReadLeb128(data, pos, leb_payload_size) ||
        leb_payload_size > data.size() - pos) {
      return std::nullopt;
    }
    payload_size = static_cast<size_t>(leb_payload_size);
  }
  obu.payload_start = pos;
  obu.end = pos + payload_size;
  return obu;
}

std::vector<uint8_t> BuildAv1MetadataObu(std::span<const uint8_t> trailer) {
  std::vector<uint8_t> payload;
  payload.reserve(trailer.size() + 1);
  WriteLeb128(kAv1MetadataType, payload);
  payload.insert(payload.end(), trailer.begin(), trailer.end());

  std::vector<uint8_t> obu;
  obu.reserve(payload.size() + 3);
  obu.push_back(
      static_cast<uint8_t>((kAv1ObuTypeMetadata << 3) | kAv1ObuSizePresentBit));
  WriteLeb128(payload.size(), obu);
  obu.insert(obu.end(), payload.begin(), payload.end());
  return obu;
}

// The metadata OBU goes after any temporal delimiter and sequence headers, so
// that it isn't mistaken for frame data.
size_t FindAv1MetadataInsertOffset(std::span<const uint8_t> data) {
  size_t pos = 0;
  size_t insert_offset = 0;
  while (pos < data.size()) {
    const std::optional<Av1Obu> obu = ReadAv1Obu(data, pos);
    if (!obu.has_value()) {
      return 0;
    }
    if (obu->type != kAv1ObuTypeTemporalDelimiter &&
        obu->type != kAv1ObuTypeSequenceHeader) {
      break;
    }
    pos = obu->end;
    insert_offset = obu->end;
  }
  return insert_offset;
}

std::vector<uint8_t> InsertAv1Trailer(std::span<const uint8_t> data,
                                      std::span<const uint8_t> trailer) {
  const std::vector<uint8_t> obu = BuildAv1MetadataObu(trailer);
  const size_t insert_offset = FindAv1MetadataInsertOffset(data);

  std::vector<uint8_t> result;
  result.reserve(data.size() + obu.size());
  result.insert(result.end(), data.begin(), data.begin() + insert_offset);
  result.insert(result.end(), obu.begin(), obu.end());
  result.insert(result.end(), data.begin() + insert_offset, data.end());
  return result;
}

std::optional<FrameMetadata> ExtractAv1Trailer(std::span<const uint8_t> data,
                                               std::vector<uint8_t>& out_data) {
  size_t pos = 0;
  while (pos < data.size()) {
    const std::optional<Av1Obu> obu = ReadAv1Obu(data, pos);
    if (!obu.has_value()) {
      return std::nullopt;
    }
    if (obu->type == kAv1ObuTypeMetadata) {
      const std::span<const uint8_t> payload =
          data.subspan(obu->payload_start, obu->end - obu->payload_start);
      size_t payload_pos = 0;
      uint64_t metadata_type = 0;
      if (ReadLeb128(payload, payload_pos, metadata_type) &&
          metadata_type == kAv1MetadataType) {
        std::optional<FrameMetadata> metadata =
            ParseTrailer(payload.subspan(payload_pos));
        if (metadata.has_value()) {
          out_data.clear();
          out_data.reserve(data.size() - (obu->end - obu->start));
          out_data.insert(out_data.end(), data.begin(),
                          data.begin() + obu->start);
          out_data.insert(out_data.end(), data.begin() + obu->end, data.end());
          return metadata;
        }
      }
    }
    pos = obu->end;
  }
  return std::nullopt;
}

}  // namespace

std::vector<uint8_t> BuildTrailer(uint64_t user_timestamp_us,
                                  uint32_t frame_id,
                                  std::span<const uint8_t> user_data) {
  const size_t fixed_size = (user_timestamp_us != 0 ? kTimestampTlvSize : 0) +
                            (frame_id != 0 ? kFrameIdTlvSize : 0) +
                            kEnvelopeSize;
  const size_t user_data_budget = kMaxTrailerSize - fixed_size - kTlvHeaderSize;
  if (user_data.size() > user_data_budget) {
    RTC_LOG(LS_WARNING) << "Dropping frame metadata user data: "
                        << user_data.size() << " bytes exceeds the budget of "
                        << user_data_budget << " bytes.";
    user_data = {};
  }

  std::vector<uint8_t> trailer =
      EncodeTrailer(user_timestamp_us, frame_id, user_data);
  if (ContainsStartCode(trailer) && !user_data.empty()) {
    RTC_LOG(LS_WARNING)
        << "Dropping frame metadata user data: it would put a start code in "
           "the trailer.";
    trailer = EncodeTrailer(user_timestamp_us, frame_id, {});
  }
  if (ContainsStartCode(trailer)) {
    RTC_LOG(LS_WARNING)
        << "Sending frame without metadata: its values would put a start code "
           "in the trailer.";
    return {};
  }
  return trailer;
}

std::optional<FrameMetadata> ParseTrailer(std::span<const uint8_t> trailer) {
  if (trailer.size() < kEnvelopeSize || trailer.size() > kMaxTrailerSize) {
    return std::nullopt;
  }
  if (!EndsWithMagic(trailer)) {
    return std::nullopt;
  }
  const size_t magic_offset = trailer.size() - sizeof(kMagic);
  const size_t trailer_size =
      static_cast<uint8_t>(trailer[magic_offset - 1] ^ kXorMask);
  if (trailer_size != trailer.size()) {
    return std::nullopt;
  }

  const std::span<const uint8_t> tlvs =
      trailer.first(trailer.size() - kEnvelopeSize);
  FrameMetadata metadata;
  bool found_field = false;
  size_t pos = 0;
  while (pos + kTlvHeaderSize <= tlvs.size()) {
    const uint8_t tag = static_cast<uint8_t>(tlvs[pos] ^ kXorMask);
    const size_t length = static_cast<uint8_t>(tlvs[pos + 1] ^ kXorMask);
    pos += kTlvHeaderSize;
    if (length > tlvs.size() - pos) {
      break;
    }

    const std::span<const uint8_t> value = tlvs.subspan(pos, length);
    if (tag == kTagTimestampUs &&
        length == sizeof(metadata.user_timestamp_us)) {
      metadata.user_timestamp_us = ReadXoredBigEndian(value);
      found_field = true;
    } else if (tag == kTagFrameId && length == sizeof(metadata.frame_id)) {
      metadata.frame_id = static_cast<uint32_t>(ReadXoredBigEndian(value));
      found_field = true;
    } else if (tag == kTagUserData) {
      metadata.user_data.clear();
      metadata.user_data.reserve(length);
      for (uint8_t byte : value) {
        metadata.user_data.push_back(static_cast<uint8_t>(byte ^ kXorMask));
      }
      found_field = true;
    }
    // Unknown tags are skipped.
    pos += length;
  }

  if (!found_field) {
    return std::nullopt;
  }
  return metadata;
}

std::vector<uint8_t> AddTrailer(std::span<const uint8_t> data,
                                std::span<const uint8_t> trailer,
                                bool is_av1) {
  if (trailer.empty()) {
    return std::vector<uint8_t>(data.begin(), data.end());
  }
  if (is_av1) {
    return InsertAv1Trailer(data, trailer);
  }
  std::vector<uint8_t> result;
  result.reserve(data.size() + trailer.size());
  result.insert(result.end(), data.begin(), data.end());
  result.insert(result.end(), trailer.begin(), trailer.end());
  return result;
}

std::optional<FrameMetadata> ExtractTrailer(std::span<const uint8_t> data,
                                            std::vector<uint8_t>& out_data,
                                            bool is_av1) {
  if (is_av1) {
    return ExtractAv1Trailer(data, out_data);
  }
  if (data.size() < kEnvelopeSize) {
    return std::nullopt;
  }
  if (!EndsWithMagic(data)) {
    return std::nullopt;
  }
  const size_t magic_offset = data.size() - sizeof(kMagic);
  const size_t trailer_size =
      static_cast<uint8_t>(data[magic_offset - 1] ^ kXorMask);
  if (trailer_size < kEnvelopeSize || trailer_size > data.size()) {
    return std::nullopt;
  }

  const size_t frame_size = data.size() - trailer_size;
  std::optional<FrameMetadata> metadata =
      ParseTrailer(data.subspan(frame_size));
  if (metadata.has_value()) {
    out_data.assign(data.begin(), data.begin() + frame_size);
  }
  return metadata;
}

bool IsAv1MimeType(std::string_view mime_type) {
  std::string lower(mime_type);
  std::transform(
      lower.begin(), lower.end(), lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find("av1") != std::string::npos;
}

}  // namespace frame_metadata

FrameMetadataTransformer::FrameMetadataTransformer(Direction direction)
    : direction_(direction) {}

FrameMetadataTransformer::~FrameMetadataTransformer() = default;

void FrameMetadataTransformer::Transform(
    std::unique_ptr<TransformableFrameInterface> frame) {
  if (enabled()) {
    if (direction_ == Direction::kSend) {
      TransformSend(*frame);
    } else {
      TransformReceive(*frame);
    }
  }
  Forward(std::move(frame));
}

void FrameMetadataTransformer::RegisterTransformedFrameCallback(
    scoped_refptr<TransformedFrameCallback> callback) {
  MutexLock lock(&mutex_);
  callback_ = std::move(callback);
}

void FrameMetadataTransformer::RegisterTransformedFrameSinkCallback(
    scoped_refptr<TransformedFrameCallback> callback,
    uint32_t ssrc) {
  MutexLock lock(&mutex_);
  sink_callbacks_[ssrc] = std::move(callback);
}

void FrameMetadataTransformer::UnregisterTransformedFrameCallback() {
  MutexLock lock(&mutex_);
  callback_ = nullptr;
}

void FrameMetadataTransformer::UnregisterTransformedFrameSinkCallback(
    uint32_t ssrc) {
  MutexLock lock(&mutex_);
  sink_callbacks_.erase(ssrc);
}

void FrameMetadataTransformer::SetEnabled(bool enabled) {
  MutexLock lock(&mutex_);
  enabled_ = enabled;
}

bool FrameMetadataTransformer::enabled() const {
  MutexLock lock(&mutex_);
  return enabled_;
}

void FrameMetadataTransformer::StoreFrameMetadata(int64_t capture_time_ms,
                                                  FrameMetadata metadata) {
  RTC_DCHECK(direction_ == Direction::kSend);
  MutexLock lock(&mutex_);
  auto it = send_map_.find(capture_time_ms);
  if (it != send_map_.end()) {
    it->second = std::move(metadata);
    return;
  }
  while (send_map_.size() >= kMaxSendEntries) {
    send_map_.erase(send_order_.front());
    send_order_.pop_front();
  }
  send_order_.push_back(capture_time_ms);
  send_map_.emplace(capture_time_ms, std::move(metadata));
}

std::optional<FrameMetadata> FrameMetadataTransformer::Lookup(
    uint32_t rtp_timestamp) const {
  MutexLock lock(&mutex_);
  auto it = receive_map_.find(rtp_timestamp);
  if (it == receive_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void FrameMetadataTransformer::TransformSend(
    TransformableFrameInterface& frame) {
  // Real send frames report an unset capture time as MinusInfinity rather than
  // nullopt.
  const std::optional<Timestamp> capture_time = frame.CaptureTime();
  if (!capture_time.has_value() || !capture_time->IsFinite()) {
    return;
  }

  std::optional<FrameMetadata> metadata;
  {
    MutexLock lock(&mutex_);
    auto it = send_map_.find(capture_time->ms());
    if (it != send_map_.end()) {
      metadata = it->second;
    }
  }
  if (!metadata.has_value()) {
    return;
  }

  const std::vector<uint8_t> trailer = frame_metadata::BuildTrailer(
      metadata->user_timestamp_us, metadata->frame_id, metadata->user_data);
  if (trailer.empty()) {
    return;
  }
  const std::vector<uint8_t> data = frame_metadata::AddTrailer(
      frame.GetData(), trailer,
      frame_metadata::IsAv1MimeType(frame.GetMimeType()));
  frame.SetData(data);
}

void FrameMetadataTransformer::TransformReceive(
    TransformableFrameInterface& frame) {
  std::vector<uint8_t> stripped_data;
  std::optional<FrameMetadata> metadata = frame_metadata::ExtractTrailer(
      frame.GetData(), stripped_data,
      frame_metadata::IsAv1MimeType(frame.GetMimeType()));
  if (!metadata.has_value()) {
    return;
  }

  const uint32_t ssrc = frame.GetSsrc();
  const uint32_t rtp_timestamp = frame.GetTimestamp();
  metadata->ssrc = ssrc;
  {
    MutexLock lock(&mutex_);
    // A different SSRC means a simulcast layer switch. The old layer's entries
    // are stale and their RTP timestamps could collide with the new layer's.
    if (active_ssrc_ != 0 && active_ssrc_ != ssrc) {
      for (auto order_it = receive_order_.begin();
           order_it != receive_order_.end();) {
        auto map_it = receive_map_.find(*order_it);
        if (map_it != receive_map_.end() && map_it->second.ssrc != ssrc) {
          receive_map_.erase(map_it);
          order_it = receive_order_.erase(order_it);
        } else {
          ++order_it;
        }
      }
    }
    active_ssrc_ = ssrc;

    auto it = receive_map_.find(rtp_timestamp);
    if (it != receive_map_.end()) {
      it->second = std::move(*metadata);
    } else {
      while (receive_map_.size() >= kMaxReceiveEntries) {
        receive_map_.erase(receive_order_.front());
        receive_order_.pop_front();
      }
      receive_order_.push_back(rtp_timestamp);
      receive_map_.emplace(rtp_timestamp, std::move(*metadata));
    }
  }
  frame.SetData(stripped_data);
}

void FrameMetadataTransformer::Forward(
    std::unique_ptr<TransformableFrameInterface> frame) {
  const uint32_t ssrc = frame->GetSsrc();
  scoped_refptr<TransformedFrameCallback> callback;
  {
    MutexLock lock(&mutex_);
    auto it = sink_callbacks_.find(ssrc);
    callback = it != sink_callbacks_.end() ? it->second : callback_;
  }
  if (callback) {
    callback->OnTransformedFrame(std::move(frame));
  } else {
    RTC_LOG(LS_WARNING) << "FrameMetadataTransformer has no callback for SSRC "
                        << ssrc << "; dropping the frame.";
  }
}

namespace {

// Passes the output of a chain's first stage to its second stage.
class ChainRelay : public TransformedFrameCallback {
 public:
  explicit ChainRelay(scoped_refptr<FrameTransformerInterface> next)
      : next_(std::move(next)) {}

  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override {
    next_->Transform(std::move(frame));
  }

 private:
  const scoped_refptr<FrameTransformerInterface> next_;
};

}  // namespace

FrameTransformerChain::FrameTransformerChain(
    scoped_refptr<FrameTransformerInterface> first,
    scoped_refptr<FrameTransformerInterface> second)
    : first_(std::move(first)),
      second_(std::move(second)),
      relay_(make_ref_counted<ChainRelay>(second_)) {
  RTC_DCHECK(first_);
  RTC_DCHECK(second_);
}

FrameTransformerChain::~FrameTransformerChain() = default;

void FrameTransformerChain::Transform(
    std::unique_ptr<TransformableFrameInterface> frame) {
  first_->Transform(std::move(frame));
}

void FrameTransformerChain::RegisterTransformedFrameCallback(
    scoped_refptr<TransformedFrameCallback> callback) {
  second_->RegisterTransformedFrameCallback(std::move(callback));
  first_->RegisterTransformedFrameCallback(relay_);
}

void FrameTransformerChain::RegisterTransformedFrameSinkCallback(
    scoped_refptr<TransformedFrameCallback> callback,
    uint32_t ssrc) {
  second_->RegisterTransformedFrameSinkCallback(std::move(callback), ssrc);
  first_->RegisterTransformedFrameSinkCallback(relay_, ssrc);
}

void FrameTransformerChain::UnregisterTransformedFrameCallback() {
  first_->UnregisterTransformedFrameCallback();
  second_->UnregisterTransformedFrameCallback();
}

void FrameTransformerChain::UnregisterTransformedFrameSinkCallback(
    uint32_t ssrc) {
  first_->UnregisterTransformedFrameSinkCallback(ssrc);
  second_->UnregisterTransformedFrameSinkCallback(ssrc);
}

}  // namespace webrtc
