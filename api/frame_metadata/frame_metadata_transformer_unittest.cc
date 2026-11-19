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

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_transformable_video_frame.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using frame_metadata::AddTrailer;
using frame_metadata::BuildTrailer;
using frame_metadata::ExtractTrailer;
using frame_metadata::IsAv1MimeType;
using frame_metadata::kEnvelopeSize;
using frame_metadata::kMaxTrailerSize;
using frame_metadata::ParseTrailer;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;

using Bytes = std::vector<uint8_t>;

constexpr uint64_t kTimestampUs = 1'790'000'000'123'456;
constexpr uint32_t kFrameId = 42;
constexpr uint32_t kSsrc = 1111;
constexpr uint32_t kOtherSsrc = 2222;
constexpr uint32_t kRtpTimestamp = 90000;
constexpr int64_t kCaptureTimeMs = 5000;

// The most user data that fits alongside a timestamp and a frame ID.
constexpr size_t kMaxUserDataWithFixedFields = 232;
constexpr size_t kMaxUserDataAlone = 248;

// AV1 OBU headers with the size-present bit set.
constexpr uint8_t kTemporalDelimiterHeader = 0b0001'0010;
constexpr uint8_t kSequenceHeaderHeader = 0b0000'1010;
constexpr uint8_t kFrameHeader = 0b0011'0010;
constexpr uint8_t kMetadataHeader = 0b0010'1010;

const Bytes kPayload = {0x10, 0x20, 0x30, 0x40};

struct FrameOptions {
  Bytes data = kPayload;
  uint32_t ssrc = kSsrc;
  uint32_t rtp_timestamp = kRtpTimestamp;
  std::optional<Timestamp> capture_time = Timestamp::Millis(kCaptureTimeMs);
  std::string mime_type = "video/VP8";
};

std::unique_ptr<NiceMock<MockTransformableVideoFrame>> MakeFrame(
    FrameOptions options = {}) {
  auto frame = std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  auto data = std::make_shared<Bytes>(std::move(options.data));
  ON_CALL(*frame, GetData).WillByDefault([data] {
    return std::span<const uint8_t>(*data);
  });
  ON_CALL(*frame, SetData)
      .WillByDefault([data](std::span<const uint8_t> bytes) {
        data->assign(bytes.begin(), bytes.end());
      });
  ON_CALL(*frame, GetSsrc).WillByDefault(Return(options.ssrc));
  ON_CALL(*frame, GetTimestamp).WillByDefault(Return(options.rtp_timestamp));
  ON_CALL(*frame, CaptureTime).WillByDefault(Return(options.capture_time));
  ON_CALL(*frame, GetMimeType).WillByDefault(Return(options.mime_type));
  return frame;
}

class RecordingCallback : public TransformedFrameCallback {
 public:
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override {
    frames_.push_back(std::move(frame));
  }

  size_t frame_count() const { return frames_.size(); }
  Bytes last_data() const {
    std::span<const uint8_t> data = frames_.back()->GetData();
    return Bytes(data.begin(), data.end());
  }

 private:
  std::vector<std::unique_ptr<TransformableFrameInterface>> frames_;
};

FrameMetadata MakeMetadata(uint64_t timestamp_us = kTimestampUs,
                           uint32_t frame_id = kFrameId,
                           Bytes user_data = {}) {
  FrameMetadata metadata;
  metadata.user_timestamp_us = timestamp_us;
  metadata.frame_id = frame_id;
  metadata.user_data = std::move(user_data);
  return metadata;
}

Bytes Concat(const Bytes& a, const Bytes& b) {
  Bytes result = a;
  result.insert(result.end(), b.begin(), b.end());
  return result;
}

Bytes Obu(uint8_t header, const Bytes& payload) {
  Bytes obu = {header, static_cast<uint8_t>(payload.size())};
  return Concat(obu, payload);
}

// BuildTrailer / ParseTrailer.

TEST(FrameMetadataTrailerTest, MatchesWireFormat) {
  const Bytes user_data = {0xAB};
  const Bytes trailer = BuildTrailer(0x0102030405060708, 0x0A0B0C0D, user_data);

  // tag ^ 0xFF, length ^ 0xFF and value ^ 0xFF, big-endian, for each field,
  // then the total length ^ 0xFF and the magic. Must stay byte-for-byte equal
  // to client-sdk-js and rust-sdks.
  const Bytes expected = {0xFE, 0xF7, 0xFE, 0xFD, 0xFC, 0xFB,
                          0xFA, 0xF9, 0xF8, 0xF7,              // timestamp
                          0xFD, 0xFB, 0xF5, 0xF4, 0xF3, 0xF2,  // frame ID
                          0xFC, 0xFE, 0x54,                    // user data
                          0xE7,                                // length
                          'L',  'K',  'T',  'S'};
  EXPECT_EQ(trailer, expected);
}

TEST(FrameMetadataTrailerTest, RoundTripsEveryFieldSubset) {
  const Bytes user_data = {1, 2, 3};
  for (int mask = 1; mask < 8; ++mask) {
    SCOPED_TRACE(mask);
    const uint64_t timestamp = (mask & 1) ? kTimestampUs : 0;
    const uint32_t frame_id = (mask & 2) ? kFrameId : 0;
    const Bytes data = (mask & 4) ? user_data : Bytes();

    const std::optional<FrameMetadata> parsed =
        ParseTrailer(BuildTrailer(timestamp, frame_id, data));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->user_timestamp_us, timestamp);
    EXPECT_EQ(parsed->frame_id, frame_id);
    EXPECT_EQ(parsed->user_data, data);
  }
}

TEST(FrameMetadataTrailerTest, NoFieldsBuildsNothing) {
  EXPECT_THAT(BuildTrailer(0, 0, {}), IsEmpty());
}

TEST(FrameMetadataTrailerTest, UserDataFillsTheBudget) {
  const Bytes with_fields(kMaxUserDataWithFixedFields, 0x42);
  Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, with_fields);
  EXPECT_EQ(trailer.size(), kMaxTrailerSize);
  EXPECT_EQ(ParseTrailer(trailer)->user_data, with_fields);

  const Bytes alone(kMaxUserDataAlone, 0x42);
  trailer = BuildTrailer(0, 0, alone);
  EXPECT_EQ(trailer.size(), kMaxTrailerSize);
  EXPECT_EQ(ParseTrailer(trailer)->user_data, alone);
}

TEST(FrameMetadataTrailerTest, DropsOversizeUserDataWithoutTruncating) {
  const Bytes oversize(kMaxUserDataWithFixedFields + 1, 0x42);
  const std::optional<FrameMetadata> parsed =
      ParseTrailer(BuildTrailer(kTimestampUs, kFrameId, oversize));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->user_timestamp_us, kTimestampUs);
  EXPECT_EQ(parsed->frame_id, kFrameId);
  EXPECT_THAT(parsed->user_data, IsEmpty());
}

TEST(FrameMetadataTrailerTest, OversizeUserDataAloneBuildsNothing) {
  EXPECT_THAT(BuildTrailer(0, 0, Bytes(kMaxUserDataAlone + 1, 0x42)),
              IsEmpty());
}

TEST(FrameMetadataTrailerTest, DropsUserDataThatFormsAStartCode) {
  const Bytes user_data = {0x42, 0xFF, 0xFF, 0xFE, 0x42};
  const std::optional<FrameMetadata> parsed =
      ParseTrailer(BuildTrailer(kTimestampUs, kFrameId, user_data));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->user_timestamp_us, kTimestampUs);
  EXPECT_EQ(parsed->frame_id, kFrameId);
  EXPECT_THAT(parsed->user_data, IsEmpty());
}

TEST(FrameMetadataTrailerTest, KeepsUserDataWithoutAStartCode) {
  // These become 00 00 02 and 00 01, which aren't start codes.
  const Bytes user_data = {0xFF, 0xFF, 0xFD, 0x42, 0xFF, 0xFE};
  EXPECT_EQ(
      ParseTrailer(BuildTrailer(kTimestampUs, kFrameId, user_data))->user_data,
      user_data);
}

TEST(FrameMetadataTrailerTest, DropsUserDataThatFormsAStartCodeWithLengthByte) {
  // With a 254-byte trailer the XORed length byte is 01, so two trailing FF
  // bytes in the user data complete 00 00 01.
  Bytes user_data(kMaxUserDataWithFixedFields - 1, 0x42);
  user_data[user_data.size() - 1] = 0xFF;
  user_data[user_data.size() - 2] = 0xFF;

  const Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, user_data);
  EXPECT_LT(trailer.size(), 254u);
  EXPECT_THAT(ParseTrailer(trailer)->user_data, IsEmpty());
}

TEST(FrameMetadataTrailerTest, FixedFieldThatFormsAStartCodeBuildsNothing) {
  // 00 FF FF FE XORs to FF 00 00 01.
  EXPECT_THAT(BuildTrailer(kTimestampUs, 0x00FFFFFE, {}), IsEmpty());
  const Bytes user_data = {1, 2, 3};
  EXPECT_THAT(BuildTrailer(kTimestampUs, 0x00FFFFFE, user_data), IsEmpty());
}

TEST(FrameMetadataTrailerTest, ParseRejectsBadInput) {
  const Bytes valid = BuildTrailer(kTimestampUs, kFrameId, {});
  ASSERT_TRUE(ParseTrailer(valid).has_value());

  EXPECT_FALSE(ParseTrailer(Bytes()).has_value());
  EXPECT_FALSE(ParseTrailer(Bytes(kEnvelopeSize - 1, 0)).has_value());
  EXPECT_FALSE(ParseTrailer(Bytes(100, 0xAA)).has_value());

  // Truncated from the front, so the length byte no longer matches.
  EXPECT_FALSE(
      ParseTrailer(std::span<const uint8_t>(valid).subspan(1)).has_value());

  Bytes wrong_magic = valid;
  wrong_magic.back() = 'X';
  EXPECT_FALSE(ParseTrailer(wrong_magic).has_value());

  Bytes wrong_length = valid;
  wrong_length[wrong_length.size() - kEnvelopeSize] ^= 0x01;
  EXPECT_FALSE(ParseTrailer(wrong_length).has_value());
}

TEST(FrameMetadataTrailerTest, ParseSkipsUnknownTags) {
  const Bytes trailer = {0x80, 0xFD, 0x11, 0x22,  // Tag 0x7F, 2 bytes.
                         0xFD, 0xFB, 0xFF, 0xFF, 0xFF, 0xD5,  // Frame ID 42.
                         0xF0,                                // Length 15.
                         'L',  'K',  'T',  'S'};
  const std::optional<FrameMetadata> parsed = ParseTrailer(trailer);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->frame_id, kFrameId);
}

TEST(FrameMetadataTrailerTest, ParseRejectsOnlyUnknownTags) {
  const Bytes trailer = {0x80, 0xFE, 0x11, 0xF7, 'L', 'K', 'T', 'S'};
  EXPECT_FALSE(ParseTrailer(trailer).has_value());
}

// AddTrailer / ExtractTrailer, appended.

TEST(FrameMetadataFrameTest, AppendsAndExtracts) {
  const Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, {});
  const Bytes framed = AddTrailer(kPayload, trailer, /*is_av1=*/false);
  EXPECT_EQ(framed, Concat(kPayload, trailer));

  Bytes stripped;
  const std::optional<FrameMetadata> metadata =
      ExtractTrailer(framed, stripped, /*is_av1=*/false);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->user_timestamp_us, kTimestampUs);
  EXPECT_EQ(stripped, kPayload);
}

TEST(FrameMetadataFrameTest, EmptyTrailerCopiesTheData) {
  EXPECT_EQ(AddTrailer(kPayload, {}, /*is_av1=*/false), kPayload);
  EXPECT_EQ(AddTrailer(kPayload, {}, /*is_av1=*/true), kPayload);
}

TEST(FrameMetadataFrameTest, ExtractLeavesOutputUntouchedOnFailure) {
  const Bytes sentinel = {9, 9};
  Bytes output = sentinel;
  EXPECT_FALSE(ExtractTrailer(kPayload, output, /*is_av1=*/false).has_value());
  EXPECT_EQ(output, sentinel);
}

TEST(FrameMetadataFrameTest, ExtractRejectsLengthLongerThanTheData) {
  // Valid magic, but the length byte says 250 bytes.
  const Bytes data = {0x05, 'L', 'K', 'T', 'S'};
  Bytes output;
  EXPECT_FALSE(ExtractTrailer(data, output, /*is_av1=*/false).has_value());
}

TEST(FrameMetadataFrameTest, ExtractRejectsAnEnvelopeWithNoFields) {
  // The magic and length byte are valid, but there are no fields to read.
  const Bytes data = {0x00, 0x00, 0x00, 0xFA, 'L', 'K', 'T', 'S'};
  Bytes output;
  EXPECT_FALSE(ExtractTrailer(data, output, /*is_av1=*/false).has_value());
}

// AV1.

TEST(FrameMetadataAv1Test, InsertsAfterTemporalDelimiter) {
  const Bytes delimiter = Obu(kTemporalDelimiterHeader, {});
  const Bytes frame_obu = Obu(kFrameHeader, {1, 2, 3});
  const Bytes data = Concat(delimiter, frame_obu);
  const Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, {});

  const Bytes framed = AddTrailer(data, trailer, /*is_av1=*/true);
  EXPECT_EQ(framed[delimiter.size()], kMetadataHeader);
  EXPECT_THAT(std::span<const uint8_t>(framed).last(frame_obu.size()),
              ElementsAreArray(frame_obu));

  Bytes stripped;
  const std::optional<FrameMetadata> metadata =
      ExtractTrailer(framed, stripped, /*is_av1=*/true);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->frame_id, kFrameId);
  EXPECT_EQ(stripped, data);
}

TEST(FrameMetadataAv1Test, InsertsAfterSequenceHeader) {
  const Bytes prefix = Concat(Obu(kTemporalDelimiterHeader, {}),
                              Obu(kSequenceHeaderHeader, {7, 7}));
  const Bytes frame_obu = Obu(kFrameHeader, {1, 2, 3});
  const Bytes data = Concat(prefix, frame_obu);
  const Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, {});

  const Bytes framed = AddTrailer(data, trailer, /*is_av1=*/true);
  EXPECT_EQ(framed[prefix.size()], kMetadataHeader);

  Bytes stripped;
  ASSERT_TRUE(ExtractTrailer(framed, stripped, /*is_av1=*/true).has_value());
  EXPECT_EQ(stripped, data);
}

TEST(FrameMetadataAv1Test, InsertsAtTheStartOfMalformedData) {
  const Bytes data = {0xFF, 0x01, 0x02};
  const Bytes trailer = BuildTrailer(kTimestampUs, kFrameId, {});
  const Bytes framed = AddTrailer(data, trailer, /*is_av1=*/true);
  EXPECT_EQ(framed[0], kMetadataHeader);
}

TEST(FrameMetadataAv1Test, ExtractIgnoresOtherMetadataTypes) {
  const Bytes data = Obu(kMetadataHeader, {0x01, 0x02});
  const Bytes sentinel = {9};
  Bytes output = sentinel;
  EXPECT_FALSE(ExtractTrailer(data, output, /*is_av1=*/true).has_value());
  EXPECT_EQ(output, sentinel);
}

TEST(FrameMetadataAv1Test, ExtractRejectsOversizeObu) {
  const Bytes data = {kFrameHeader, 0x50, 0x01};
  Bytes output;
  EXPECT_FALSE(ExtractTrailer(data, output, /*is_av1=*/true).has_value());
}

TEST(FrameMetadataAv1Test, DetectsMimeTypeInAnyCase) {
  EXPECT_TRUE(IsAv1MimeType("video/AV1"));
  EXPECT_TRUE(IsAv1MimeType("video/av1"));
  EXPECT_FALSE(IsAv1MimeType("video/VP8"));
  EXPECT_FALSE(IsAv1MimeType("video/H264"));
}

// Send.

class FrameMetadataSendTest : public ::testing::Test {
 protected:
  FrameMetadataSendTest()
      : transformer_(make_ref_counted<FrameMetadataTransformer>(
            FrameMetadataTransformer::Direction::kSend)),
        callback_(make_ref_counted<RecordingCallback>()) {
    transformer_->RegisterTransformedFrameCallback(callback_);
  }

  const scoped_refptr<FrameMetadataTransformer> transformer_;
  const scoped_refptr<RecordingCallback> callback_;
};

TEST_F(FrameMetadataSendTest, WritesStoredValues) {
  const Bytes user_data = {1, 2, 3};
  transformer_->StoreFrameMetadata(
      kCaptureTimeMs, MakeMetadata(kTimestampUs, kFrameId, user_data));

  transformer_->Transform(MakeFrame());

  ASSERT_EQ(callback_->frame_count(), 1u);
  Bytes stripped;
  const std::optional<FrameMetadata> metadata =
      ExtractTrailer(callback_->last_data(), stripped, /*is_av1=*/false);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->user_timestamp_us, kTimestampUs);
  EXPECT_EQ(metadata->frame_id, kFrameId);
  EXPECT_EQ(metadata->user_data, user_data);
  EXPECT_EQ(stripped, kPayload);
}

TEST_F(FrameMetadataSendTest, GeneratesNothingItself) {
  // Only the stored field goes out, even though the frame has a capture time.
  transformer_->StoreFrameMetadata(kCaptureTimeMs,
                                   MakeMetadata(/*timestamp_us=*/0, kFrameId));

  transformer_->Transform(MakeFrame());

  Bytes stripped;
  const std::optional<FrameMetadata> metadata =
      ExtractTrailer(callback_->last_data(), stripped, /*is_av1=*/false);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->user_timestamp_us, 0u);
  EXPECT_EQ(metadata->frame_id, kFrameId);
}

TEST_F(FrameMetadataSendTest, EveryLayerGetsTheSameValues) {
  transformer_->StoreFrameMetadata(kCaptureTimeMs, MakeMetadata());

  for (uint32_t ssrc : {kSsrc, kOtherSsrc}) {
    transformer_->Transform(MakeFrame({.ssrc = ssrc}));
    Bytes stripped;
    const std::optional<FrameMetadata> metadata =
        ExtractTrailer(callback_->last_data(), stripped, /*is_av1=*/false);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->frame_id, kFrameId);
  }
}

TEST_F(FrameMetadataSendTest, UsesTheMetadataObuForAv1) {
  transformer_->StoreFrameMetadata(kCaptureTimeMs, MakeMetadata());
  const Bytes data = Obu(kFrameHeader, {1, 2, 3});

  transformer_->Transform(MakeFrame({.data = data, .mime_type = "video/AV1"}));

  const Bytes output = callback_->last_data();
  EXPECT_EQ(output[0], kMetadataHeader);
  Bytes stripped;
  ASSERT_TRUE(ExtractTrailer(output, stripped, /*is_av1=*/true).has_value());
  EXPECT_EQ(stripped, data);
}

TEST_F(FrameMetadataSendTest, EvictsTheOldestEntries) {
  const int64_t newest =
      static_cast<int64_t>(FrameMetadataTransformer::kMaxSendEntries) + 1;
  for (int64_t capture_time_ms = 1; capture_time_ms <= newest;
       ++capture_time_ms) {
    transformer_->StoreFrameMetadata(capture_time_ms, MakeMetadata());
  }

  transformer_->Transform(MakeFrame({.capture_time = Timestamp::Millis(1)}));
  EXPECT_EQ(callback_->last_data(), kPayload);

  transformer_->Transform(
      MakeFrame({.capture_time = Timestamp::Millis(newest)}));
  EXPECT_NE(callback_->last_data(), kPayload);
}

TEST_F(FrameMetadataSendTest, StoringTheSameCaptureTimeReplacesTheValues) {
  transformer_->StoreFrameMetadata(kCaptureTimeMs,
                                   MakeMetadata(kTimestampUs, 1));
  transformer_->StoreFrameMetadata(kCaptureTimeMs,
                                   MakeMetadata(kTimestampUs, 2));

  transformer_->Transform(MakeFrame());

  Bytes stripped;
  EXPECT_EQ(ExtractTrailer(callback_->last_data(), stripped, /*is_av1=*/false)
                ->frame_id,
            2u);
}

TEST_F(FrameMetadataSendTest, NoStoredValuesMeansNoTrailer) {
  transformer_->Transform(MakeFrame());
  ASSERT_EQ(callback_->frame_count(), 1u);
  EXPECT_EQ(callback_->last_data(), kPayload);
}

TEST_F(FrameMetadataSendTest, UnsetCaptureTimeMeansNoTrailer) {
  transformer_->StoreFrameMetadata(kCaptureTimeMs, MakeMetadata());

  transformer_->Transform(MakeFrame({.capture_time = std::nullopt}));
  EXPECT_EQ(callback_->last_data(), kPayload);

  // This is how real send frames report an unset capture time.
  transformer_->Transform(
      MakeFrame({.capture_time = Timestamp::MinusInfinity()}));
  EXPECT_EQ(callback_->frame_count(), 2u);
  EXPECT_EQ(callback_->last_data(), kPayload);
}

TEST_F(FrameMetadataSendTest, DisabledMeansNoTrailer) {
  transformer_->StoreFrameMetadata(kCaptureTimeMs, MakeMetadata());
  transformer_->SetEnabled(false);
  EXPECT_FALSE(transformer_->enabled());

  transformer_->Transform(MakeFrame());

  ASSERT_EQ(callback_->frame_count(), 1u);
  EXPECT_EQ(callback_->last_data(), kPayload);
}

TEST_F(FrameMetadataSendTest, PrefersTheCallbackForTheFramesSsrc) {
  auto sink = make_ref_counted<RecordingCallback>();
  transformer_->RegisterTransformedFrameSinkCallback(sink, kSsrc);

  transformer_->Transform(MakeFrame({.ssrc = kSsrc}));
  EXPECT_EQ(sink->frame_count(), 1u);
  EXPECT_EQ(callback_->frame_count(), 0u);

  transformer_->Transform(MakeFrame({.ssrc = kOtherSsrc}));
  EXPECT_EQ(sink->frame_count(), 1u);
  EXPECT_EQ(callback_->frame_count(), 1u);

  transformer_->UnregisterTransformedFrameSinkCallback(kSsrc);
  transformer_->Transform(MakeFrame({.ssrc = kSsrc}));
  EXPECT_EQ(sink->frame_count(), 1u);
  EXPECT_EQ(callback_->frame_count(), 2u);
}

// Receive.

class FrameMetadataReceiveTest : public ::testing::Test {
 protected:
  FrameMetadataReceiveTest()
      : transformer_(make_ref_counted<FrameMetadataTransformer>(
            FrameMetadataTransformer::Direction::kReceive)),
        callback_(make_ref_counted<RecordingCallback>()) {
    transformer_->RegisterTransformedFrameCallback(callback_);
  }

  void ReceiveFrame(uint32_t ssrc,
                    uint32_t rtp_timestamp,
                    uint32_t frame_id = kFrameId) {
    const Bytes trailer = BuildTrailer(kTimestampUs, frame_id, {});
    transformer_->Transform(MakeFrame({.data = Concat(kPayload, trailer),
                                       .ssrc = ssrc,
                                       .rtp_timestamp = rtp_timestamp}));
  }

  const scoped_refptr<FrameMetadataTransformer> transformer_;
  const scoped_refptr<RecordingCallback> callback_;
};

TEST_F(FrameMetadataReceiveTest, StripsAndCaches) {
  ReceiveFrame(kSsrc, kRtpTimestamp);

  ASSERT_EQ(callback_->frame_count(), 1u);
  EXPECT_EQ(callback_->last_data(), kPayload);
  const std::optional<FrameMetadata> metadata =
      transformer_->Lookup(kRtpTimestamp);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->user_timestamp_us, kTimestampUs);
  EXPECT_EQ(metadata->frame_id, kFrameId);
  EXPECT_EQ(metadata->ssrc, kSsrc);
}

TEST_F(FrameMetadataReceiveTest, LookupKeepsTheEntry) {
  ReceiveFrame(kSsrc, kRtpTimestamp);
  EXPECT_TRUE(transformer_->Lookup(kRtpTimestamp).has_value());
  EXPECT_TRUE(transformer_->Lookup(kRtpTimestamp).has_value());
}

TEST_F(FrameMetadataReceiveTest, LookupOfUnknownTimestampIsEmpty) {
  EXPECT_FALSE(transformer_->Lookup(kRtpTimestamp).has_value());
}

TEST_F(FrameMetadataReceiveTest, FramesWithoutATrailerPassThrough) {
  transformer_->Transform(MakeFrame());
  ASSERT_EQ(callback_->frame_count(), 1u);
  EXPECT_EQ(callback_->last_data(), kPayload);
  EXPECT_FALSE(transformer_->Lookup(kRtpTimestamp).has_value());
}

TEST_F(FrameMetadataReceiveTest, EvictsTheOldestEntries) {
  const uint32_t count = FrameMetadataTransformer::kMaxReceiveEntries + 1;
  for (uint32_t i = 0; i < count; ++i) {
    ReceiveFrame(kSsrc, 1000 + i);
  }
  EXPECT_FALSE(transformer_->Lookup(1000).has_value());
  EXPECT_TRUE(transformer_->Lookup(1001).has_value());
  EXPECT_TRUE(transformer_->Lookup(1000 + count - 1).has_value());
}

TEST_F(FrameMetadataReceiveTest, RepeatedTimestampDoesNotBreakEviction) {
  const uint32_t max = FrameMetadataTransformer::kMaxReceiveEntries;
  for (uint32_t i = 0; i < max; ++i) {
    ReceiveFrame(kSsrc, 1000 + i);
  }
  // The oldest entry again, with new values, at capacity.
  ReceiveFrame(kSsrc, 1000, /*frame_id=*/7);
  EXPECT_EQ(transformer_->Lookup(1000)->frame_id, 7u);

  // The next new timestamp evicts it as the oldest.
  ReceiveFrame(kSsrc, 1000 + max);
  EXPECT_FALSE(transformer_->Lookup(1000).has_value());
  EXPECT_TRUE(transformer_->Lookup(1001).has_value());
}

TEST_F(FrameMetadataReceiveTest, SsrcChangeFlushesOtherLayers) {
  ReceiveFrame(kSsrc, 1000);
  ReceiveFrame(kSsrc, 2000);
  ReceiveFrame(kOtherSsrc, 3000);

  EXPECT_FALSE(transformer_->Lookup(1000).has_value());
  EXPECT_FALSE(transformer_->Lookup(2000).has_value());
  EXPECT_TRUE(transformer_->Lookup(3000).has_value());
}

TEST_F(FrameMetadataReceiveTest, DisabledDoesNotStrip) {
  transformer_->SetEnabled(false);
  const Bytes framed =
      Concat(kPayload, BuildTrailer(kTimestampUs, kFrameId, {}));

  transformer_->Transform(MakeFrame({.data = framed}));

  ASSERT_EQ(callback_->frame_count(), 1u);
  EXPECT_EQ(callback_->last_data(), framed);
  EXPECT_FALSE(transformer_->Lookup(kRtpTimestamp).has_value());
}

TEST_F(FrameMetadataReceiveTest, RoundTripsFromASendTransformer) {
  auto sender = make_ref_counted<FrameMetadataTransformer>(
      FrameMetadataTransformer::Direction::kSend);
  auto sent = make_ref_counted<RecordingCallback>();
  sender->RegisterTransformedFrameCallback(sent);
  sender->StoreFrameMetadata(kCaptureTimeMs, MakeMetadata());
  sender->Transform(MakeFrame());

  transformer_->Transform(MakeFrame({.data = sent->last_data()}));

  EXPECT_EQ(callback_->last_data(), kPayload);
  EXPECT_EQ(transformer_->Lookup(kRtpTimestamp)->frame_id, kFrameId);
}

// FrameTransformerChain.

// A stage that appends a marker byte and records its registrations. Like the
// frame cryptor, it only forwards through the kind of callback it was given.
class FakeStage : public FrameTransformerInterface {
 public:
  explicit FakeStage(uint8_t marker, bool* destroyed = nullptr)
      : marker_(marker), destroyed_(destroyed) {}
  ~FakeStage() override {
    if (destroyed_ != nullptr) {
      *destroyed_ = true;
    }
  }

  void Transform(std::unique_ptr<TransformableFrameInterface> frame) override {
    std::span<const uint8_t> data = frame->GetData();
    Bytes with_marker(data.begin(), data.end());
    with_marker.push_back(marker_);
    frame->SetData(with_marker);

    const uint32_t ssrc = frame->GetSsrc();
    auto it = sink_callbacks_.find(ssrc);
    if (it != sink_callbacks_.end()) {
      it->second->OnTransformedFrame(std::move(frame));
    } else if (callback_) {
      callback_->OnTransformedFrame(std::move(frame));
    }
  }
  void RegisterTransformedFrameCallback(
      scoped_refptr<TransformedFrameCallback> callback) override {
    callback_ = std::move(callback);
  }
  void RegisterTransformedFrameSinkCallback(
      scoped_refptr<TransformedFrameCallback> callback,
      uint32_t ssrc) override {
    sink_callbacks_[ssrc] = std::move(callback);
  }
  void UnregisterTransformedFrameCallback() override { callback_ = nullptr; }
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
    sink_callbacks_.erase(ssrc);
  }

  bool has_callback() const { return callback_ != nullptr; }
  size_t sink_callback_count() const { return sink_callbacks_.size(); }

 private:
  const uint8_t marker_;
  bool* const destroyed_;
  scoped_refptr<TransformedFrameCallback> callback_;
  std::map<uint32_t, scoped_refptr<TransformedFrameCallback>> sink_callbacks_;
};

TEST(FrameTransformerChainTest, RunsStagesInTheGivenOrder) {
  auto chain = make_ref_counted<FrameTransformerChain>(
      make_ref_counted<FakeStage>('a'), make_ref_counted<FakeStage>('b'));
  auto callback = make_ref_counted<RecordingCallback>();
  chain->RegisterTransformedFrameCallback(callback);

  chain->Transform(MakeFrame({.data = Bytes()}));

  ASSERT_EQ(callback->frame_count(), 1u);
  EXPECT_THAT(callback->last_data(), ElementsAre('a', 'b'));
}

TEST(FrameTransformerChainTest, ForwardsPerSsrcCallbacks) {
  auto first = make_ref_counted<FakeStage>('a');
  auto second = make_ref_counted<FakeStage>('b');
  auto chain = make_ref_counted<FrameTransformerChain>(first, second);
  auto callback = make_ref_counted<RecordingCallback>();

  chain->RegisterTransformedFrameSinkCallback(callback, kSsrc);

  EXPECT_FALSE(first->has_callback());
  EXPECT_FALSE(second->has_callback());
  EXPECT_EQ(first->sink_callback_count(), 1u);
  EXPECT_EQ(second->sink_callback_count(), 1u);

  chain->Transform(MakeFrame({.data = Bytes()}));
  EXPECT_THAT(callback->last_data(), ElementsAre('a', 'b'));

  chain->UnregisterTransformedFrameSinkCallback(kSsrc);
  EXPECT_EQ(first->sink_callback_count(), 0u);
  EXPECT_EQ(second->sink_callback_count(), 0u);
}

TEST(FrameTransformerChainTest, ForwardsGlobalCallbacks) {
  auto first = make_ref_counted<FakeStage>('a');
  auto second = make_ref_counted<FakeStage>('b');
  auto chain = make_ref_counted<FrameTransformerChain>(first, second);

  chain->RegisterTransformedFrameCallback(
      make_ref_counted<RecordingCallback>());
  EXPECT_TRUE(first->has_callback());
  EXPECT_TRUE(second->has_callback());
  EXPECT_EQ(first->sink_callback_count(), 0u);
  EXPECT_EQ(second->sink_callback_count(), 0u);

  chain->UnregisterTransformedFrameCallback();
  EXPECT_FALSE(first->has_callback());
  EXPECT_FALSE(second->has_callback());
}

TEST(FrameTransformerChainTest, Nests) {
  auto inner = make_ref_counted<FrameTransformerChain>(
      make_ref_counted<FakeStage>('a'), make_ref_counted<FakeStage>('b'));
  auto outer = make_ref_counted<FrameTransformerChain>(
      inner, make_ref_counted<FakeStage>('c'));
  auto callback = make_ref_counted<RecordingCallback>();
  outer->RegisterTransformedFrameSinkCallback(callback, kSsrc);

  outer->Transform(MakeFrame({.data = Bytes()}));

  EXPECT_THAT(callback->last_data(), ElementsAre('a', 'b', 'c'));
}

TEST(FrameTransformerChainTest, HasNoReferenceCycle) {
  // If the chain registered itself with `first`, `first` and the chain would
  // hold each other and neither stage would ever be destroyed.
  bool first_destroyed = false;
  bool second_destroyed = false;
  scoped_refptr<FrameTransformerChain> chain =
      make_ref_counted<FrameTransformerChain>(
          make_ref_counted<FakeStage>('a', &first_destroyed),
          make_ref_counted<FakeStage>('b', &second_destroyed));
  chain->RegisterTransformedFrameCallback(
      make_ref_counted<RecordingCallback>());

  chain = nullptr;

  EXPECT_TRUE(first_destroyed);
  EXPECT_TRUE(second_destroyed);
}

}  // namespace
}  // namespace webrtc
