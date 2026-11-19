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

#ifndef API_FRAME_METADATA_FRAME_METADATA_TRANSFORMER_H_
#define API_FRAME_METADATA_FRAME_METADATA_TRANSFORMER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Per-frame metadata carried in a LiveKit packet trailer ("LKTS").
//
// The wire format is shared with client-sdk-js and rust-sdks and must stay
// byte-for-byte identical to them:
//
//   [TLV]* [trailer_len: 1B] ['L' 'K' 'T' 'S']
//
// Every TLV byte and the length byte are XORed with 0xFF so that H.264 start
// codes cannot appear inside the trailer. trailer_len covers the whole
// trailer, envelope included, so a trailer is at most 255 bytes. For AV1 the
// same trailer bytes are carried in a metadata OBU instead of being appended.
struct FrameMetadata {
  uint64_t user_timestamp_us = 0;  // tag 0x01; 0 means unset
  uint32_t frame_id = 0;           // tag 0x02; 0 means unset
  uint32_t ssrc = 0;               // receive side only; not on the wire
  std::vector<uint8_t> user_data;  // tag 0x03; empty means unset
};

namespace frame_metadata {

inline constexpr uint8_t kMagic[4] = {'L', 'K', 'T', 'S'};
// [trailer_len: 1B] [magic: 4B]
inline constexpr size_t kEnvelopeSize = 5;
inline constexpr size_t kMaxTrailerSize = 255;

inline constexpr uint8_t kTagTimestampUs = 0x01;
inline constexpr uint8_t kTagFrameId = 0x02;
inline constexpr uint8_t kTagUserData = 0x03;

inline constexpr size_t kTlvHeaderSize = 2;
inline constexpr size_t kTimestampTlvSize = kTlvHeaderSize + 8;
inline constexpr size_t kFrameIdTlvSize = kTlvHeaderSize + 4;

// AV1 metadata OBU type used to carry the trailer (same as rust-sdks).
inline constexpr uint64_t kAv1MetadataType = 31;

// Builds a trailer for the fields that are set. Returns an empty vector when
// no field is set. user_data is dropped, never truncated, if it doesn't fit
// the 255-byte budget or if the XORed trailer would contain an H.264 start
// code (00 00 01), because the packetizer would split the frame there. If the
// trailer still contains a start code without user_data, returns an empty
// vector.
RTC_EXPORT std::vector<uint8_t> BuildTrailer(
    uint64_t user_timestamp_us,
    uint32_t frame_id,
    std::span<const uint8_t> user_data);

// Parses a complete trailer (TLV region + envelope, nothing before it).
// Returns nullopt if the envelope is invalid or no known TLV is present.
RTC_EXPORT std::optional<FrameMetadata> ParseTrailer(
    std::span<const uint8_t> trailer);

// Adds `trailer` to an encoded frame: appended for most codecs, inserted as a
// metadata OBU for AV1. Returns a copy of `data` when `trailer` is empty.
RTC_EXPORT std::vector<uint8_t> AddTrailer(std::span<const uint8_t> data,
                                           std::span<const uint8_t> trailer,
                                           bool is_av1);

// Removes a trailer from an encoded frame. On success returns the metadata
// and writes the frame without the trailer to `out_data`. Otherwise returns
// nullopt and leaves `out_data` untouched.
RTC_EXPORT std::optional<FrameMetadata> ExtractTrailer(
    std::span<const uint8_t> data,
    std::vector<uint8_t>& out_data,
    bool is_av1);

RTC_EXPORT bool IsAv1MimeType(std::string_view mime_type);

}  // namespace frame_metadata

// Frame transformer that writes (send) or strips (receive) packet trailers.
//
// Creating a transformer doesn't install it. Install it, alone or inside a
// FrameTransformerChain, with SetFrameTransformer() on the sender or receiver.
//
// Send: writes the values given to StoreFrameMetadata() and never generates
// any. Values are found by the frame's capture time, so all simulcast layers
// of a captured frame get the same ones. A frame with no stored values, or
// without a capture time, goes out without a trailer.
//
// Receive: strips the trailer and caches the metadata by RTP timestamp, so
// that a decoded frame can be matched to it with Lookup(). The cache holds
// the latest kMaxReceiveEntries frames and is flushed of other SSRCs'
// entries when the SSRC changes (simulcast layer switch). Frames without a
// trailer pass through unchanged.
//
// With E2EE, the trailer has to stay outside the encrypted payload, so chain
// this with FrameCryptorTransformer in this order: send = cryptor then
// metadata, receive = metadata then cryptor.
class RTC_EXPORT FrameMetadataTransformer : public FrameTransformerInterface {
 public:
  enum class Direction { kSend, kReceive };

  static constexpr size_t kMaxSendEntries = 300;
  static constexpr size_t kMaxReceiveEntries = 300;

  explicit FrameMetadataTransformer(Direction direction);
  ~FrameMetadataTransformer() override;

  // FrameTransformerInterface.
  void Transform(std::unique_ptr<TransformableFrameInterface> frame) override;
  void RegisterTransformedFrameCallback(
      scoped_refptr<TransformedFrameCallback> callback) override;
  void RegisterTransformedFrameSinkCallback(
      scoped_refptr<TransformedFrameCallback> callback,
      uint32_t ssrc) override;
  void UnregisterTransformedFrameCallback() override;
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

  // When disabled, frames pass through untouched in both directions. A disabled
  // receive transformer doesn't strip trailers, so a frame that carries one
  // reaches the decoder with it attached.
  void SetEnabled(bool enabled);
  bool enabled() const;

  Direction direction() const { return direction_; }

  // Send side: values for the frame with this capture time, in milliseconds.
  // Call before the frame reaches the encoder. Entries stay when read, because
  // each simulcast layer reads them, and the oldest are dropped past
  // kMaxSendEntries. WebRTC clamps a timestamp that's ahead of its clock, so
  // such a frame misses its entry and gets no trailer.
  void StoreFrameMetadata(int64_t capture_time_ms, FrameMetadata metadata);

  // Receive side: metadata for the frame with this RTP timestamp, if it is
  // still cached. Does not remove the entry, so a frame rendered more than
  // once still finds its metadata.
  std::optional<FrameMetadata> Lookup(uint32_t rtp_timestamp) const;

 private:
  void TransformSend(TransformableFrameInterface& frame);
  void TransformReceive(TransformableFrameInterface& frame);
  void Forward(std::unique_ptr<TransformableFrameInterface> frame);

  const Direction direction_;

  mutable Mutex mutex_;
  bool enabled_ RTC_GUARDED_BY(mutex_) = true;
  scoped_refptr<TransformedFrameCallback> callback_ RTC_GUARDED_BY(mutex_);
  std::map<uint32_t, scoped_refptr<TransformedFrameCallback>> sink_callbacks_
      RTC_GUARDED_BY(mutex_);

  // Send side, keyed by capture time in ms.
  std::unordered_map<int64_t, FrameMetadata> send_map_ RTC_GUARDED_BY(mutex_);
  std::deque<int64_t> send_order_ RTC_GUARDED_BY(mutex_);

  // Receive side.
  std::unordered_map<uint32_t, FrameMetadata> receive_map_
      RTC_GUARDED_BY(mutex_);
  std::deque<uint32_t> receive_order_ RTC_GUARDED_BY(mutex_);
  uint32_t active_ssrc_ RTC_GUARDED_BY(mutex_) = 0;
};

// Runs every frame through `first`, then `second`, then the callback the
// stream registered. The chain doesn't know what its stages are and doesn't
// reorder them by direction, so the caller sets the order. A chain is itself a
// transformer, so chains can nest.
//
// Stages get the same kind of callback registration the stream makes, global or
// per-SSRC, because a stage may only use one kind. FrameCryptorTransformer uses
// per-SSRC callbacks for video and the global callback for audio.
class RTC_EXPORT FrameTransformerChain : public FrameTransformerInterface {
 public:
  FrameTransformerChain(scoped_refptr<FrameTransformerInterface> first,
                        scoped_refptr<FrameTransformerInterface> second);
  ~FrameTransformerChain() override;

  void Transform(std::unique_ptr<TransformableFrameInterface> frame) override;
  void RegisterTransformedFrameCallback(
      scoped_refptr<TransformedFrameCallback> callback) override;
  void RegisterTransformedFrameSinkCallback(
      scoped_refptr<TransformedFrameCallback> callback,
      uint32_t ssrc) override;
  void UnregisterTransformedFrameCallback() override;
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

  const scoped_refptr<FrameTransformerInterface>& first() const {
    return first_;
  }
  const scoped_refptr<FrameTransformerInterface>& second() const {
    return second_;
  }

 private:
  const scoped_refptr<FrameTransformerInterface> first_;
  const scoped_refptr<FrameTransformerInterface> second_;
  // Registered as `first_`'s output callback. Holds `second_` but not the
  // chain, so there is no reference cycle.
  const scoped_refptr<TransformedFrameCallback> relay_;
};

}  // namespace webrtc

#endif  // API_FRAME_METADATA_FRAME_METADATA_TRANSFORMER_H_
