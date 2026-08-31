/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_stun_piggyback_controller_interface.h"
#include "p2p/dtls/dtls_stun_piggyback_controller_sped.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
// Extracted from a stock DTLS call using Wireshark.
// Each packet (apart from the last) is truncated to
// the first fragment to keep things short.

// Based on a "server hello done" but with different msg_seq.
const std::vector<uint8_t> dtls_flight1 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x01,                                            // seq=1
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00,  // msg_seq=0x1234
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight2 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x02,                                            // seq=2
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x43, 0x21, 0x00,  // msg_seq=0x4321
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight3 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x03,                                            // seq=3
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00,  // msg_seq=0x4444
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight4 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x04,                                            // seq=4
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x54, 0x86, 0x00,  // msg_seq=0x5486
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> empty = {};

const std::vector<uint8_t> kPayload = {0x1, 0x2, 0x3};

// The two implementations of the piggybacking protocol. They agree on which
// handshake flights are piggybacked and only differ in how the piggybacking
// session terminates.
enum class Variant { kGoogSped, kSped };

static_assert(DtlsStunPiggybackController::kMaxAckSize ==
              DtlsStunPiggybackControllerSped::kMaxAckSize);

std::unique_ptr<DtlsStunPiggybackControllerInterface> CreateController(
    Variant variant,
    absl::AnyInvocable<void(std::span<const uint8_t>)> dtls_data_callback,
    // NOLINTNEXTLINE(readability/casting) - not a cast; false positive!
    absl::AnyInvocable<void(bool) &&> piggyback_complete_callback) {
  if (variant == Variant::kSped) {
    return std::make_unique<DtlsStunPiggybackControllerSped>(
        std::move(dtls_data_callback), std::move(piggyback_complete_callback));
  }
  return std::make_unique<DtlsStunPiggybackController>(
      std::move(dtls_data_callback), std::move(piggyback_complete_callback));
}

std::vector<uint32_t> FromAckAttribute(std::span<uint8_t> attr) {
  ByteBufferReader ack_reader(attr);
  std::vector<uint32_t> values;
  uint32_t value;
  while (ack_reader.ReadUInt32(&value)) {
    values.push_back(value);
  }
  RTC_DCHECK_EQ(ack_reader.Length(), 0);
  return values;
}

std::vector<uint8_t> FakeDtlsPacket(uint16_t packet_number) {
  auto packet = dtls_flight1;
  packet[17] = static_cast<uint8_t>(packet_number >> 8);
  packet[18] = static_cast<uint8_t>(packet_number & 255);
  return packet;
}

std::unique_ptr<StunByteStringAttribute> WrapInStun(IceAttributeType type,
                                                    absl::string_view data) {
  return std::make_unique<StunByteStringAttribute>(type, data);
}

std::unique_ptr<StunByteStringAttribute> WrapInStun(
    IceAttributeType type,
    const std::vector<uint8_t>& data) {
  return std::make_unique<StunByteStringAttribute>(type, data);
}

std::unique_ptr<StunByteStringAttribute> WrapInStun(
    IceAttributeType type,
    const std::vector<uint32_t>& data) {
  return std::make_unique<StunByteStringAttribute>(type, data);
}

}  // namespace

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::SizeIs;
using State = DtlsStunPiggybackControllerInterface::State;

class DtlsStunPiggybackControllerTestBase : public ::testing::Test {
 protected:
  explicit DtlsStunPiggybackControllerTestBase(Variant variant)
      : client_(CreateController(
            variant,
            [this](std::span<const uint8_t> data) { ClientPacketSink(data); },
            [this](bool success) { ClientCompleteCallback(success); })),
        server_(CreateController(
            variant,
            [this](std::span<const uint8_t> data) { ServerPacketSink(data); },
            [this](bool success) { ServerCompleteCallback(success); })),
        packet_(kPayload, SocketAddress(), std::nullopt) {}

  // Send from client to server embedded in STUN.
  void SendClientToServerEmbedded(const std::vector<uint8_t>& packet,
                                  StunMessageType type) {
    if (!packet.empty()) {
      client_->CapturePacket(packet);
      client_->Flush();
    } else {
      client_->ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    std::optional<std::span<uint8_t>> view_data;
    if (auto data = client_->GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
      view_data = attr_data->array_view();
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    std::optional<std::vector<uint32_t>> view_acks;
    if (auto ack = client_->GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
      view_acks = FromAckAttribute(attr_ack->array_view());
    }
    server_->ReportDataPiggybacked(view_data, view_acks);
  }
  // Send from client to server as plain DTLS.
  void SendClientToServerDtls(const std::vector<uint8_t> packet) {
    if (!packet.empty()) {
      client_->CapturePacket(packet);
      client_->Flush();
    } else {
      client_->ClearCachedPacketForTesting();
    }
    server_->ReportDtlsPacket(packet);
  }
  // Send from server to client embedded in STUN
  void SendServerToClientEmbedded(const std::vector<uint8_t>& packet,
                                  StunMessageType type) {
    if (!packet.empty()) {
      server_->CapturePacket(packet);
      server_->Flush();
    } else {
      server_->ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    std::optional<std::span<uint8_t>> view_data;
    if (auto data = server_->GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
      view_data = attr_data->array_view();
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    std::optional<std::vector<uint32_t>> view_acks;
    if (auto ack = server_->GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
      view_acks = FromAckAttribute(attr_ack->array_view());
    }
    client_->ReportDataPiggybacked(view_data, view_acks);
    MaybeSetHandshakeComplete(packet);
  }
  // Send from server to client as plain DTLS.
  void SendServerToClientDtls(const std::vector<uint8_t> packet) {
    if (!packet.empty()) {
      server_->CapturePacket(packet);
      server_->Flush();
    } else {
      server_->ClearCachedPacketForTesting();
    }
    client_->ReportDtlsPacket(packet);
    MaybeSetHandshakeComplete(packet);
  }

  void DisableSupport(DtlsStunPiggybackControllerInterface& client_or_server) {
    ASSERT_EQ(client_or_server.state(), State::TENTATIVE);
    client_or_server.ReportDataPiggybacked(std::nullopt, std::nullopt);
    ASSERT_EQ(client_or_server.state(), State::OFF);
  }

  std::unique_ptr<DtlsStunPiggybackControllerInterface> client_;
  std::unique_ptr<DtlsStunPiggybackControllerInterface> server_;

  MOCK_METHOD(void, ClientPacketSink, (std::span<const uint8_t>));
  MOCK_METHOD(void, ServerPacketSink, (std::span<const uint8_t>));

  MOCK_METHOD(void, ClientCompleteCallback, (bool));
  MOCK_METHOD(void, ServerCompleteCallback, (bool));

  ReceivedIpPacket packet_;

 private:
  void MaybeSetHandshakeComplete(std::vector<uint8_t> packet) {
    // Note: this assumes DTLS 1.2
    if (packet == dtls_flight4) {
      // After sending flight 4, the server handshake is complete.
      server_->SetDtlsHandshakeComplete(/*is_client=*/false,
                                        /*is_dtls13=*/false);
      // When receiving flight 4, client handshake is complete.
      client_->SetDtlsHandshakeComplete(/*is_client=*/true,
                                        /*is_dtls13=*/false);
    }
  }
};

// Behaviour shared by both variants.
class DtlsStunPiggybackControllerTest
    : public DtlsStunPiggybackControllerTestBase,
      public ::testing::WithParamInterface<Variant> {
 protected:
  DtlsStunPiggybackControllerTest()
      : DtlsStunPiggybackControllerTestBase(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         DtlsStunPiggybackControllerTest,
                         ::testing::Values(Variant::kGoogSped, Variant::kSped),
                         [](const ::testing::TestParamInfo<Variant>& info) {
                           return info.param == Variant::kSped ? "Sped"
                                                               : "GoogSped";
                         });

// Behaviour of the default variant only.
class DtlsStunPiggybackControllerGoogSpedTest
    : public DtlsStunPiggybackControllerTestBase {
 protected:
  DtlsStunPiggybackControllerGoogSpedTest()
      : DtlsStunPiggybackControllerTestBase(Variant::kGoogSped) {}
};

// Behaviour of the variant behind WebRTC-DtlsStunPiggybackControllerSped only.
class DtlsStunPiggybackControllerSpedTest
    : public DtlsStunPiggybackControllerTestBase {
 protected:
  DtlsStunPiggybackControllerSpedTest()
      : DtlsStunPiggybackControllerTestBase(Variant::kSped) {}
};

TEST_P(DtlsStunPiggybackControllerTest, BasicHandshake) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::COMPLETE);
}

TEST_P(DtlsStunPiggybackControllerTest, FirstClientPacketLost) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 2+3
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 4
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_->state(), State::COMPLETE);
}

TEST_P(DtlsStunPiggybackControllerTest, NotSupportedByServer) {
  DisableSupport(*server_);

  // Flight 1
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  // TODO: bugs.webrtc.org/367395350 - assert when calling the complete
  // callback in this case which currently causes a sleuth of test failures.
  // EXPECT_CALL(*this, ClientCompleteCallback());
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::OFF);
}

TEST_P(DtlsStunPiggybackControllerTest, NotSupportedByServerClientReceives) {
  DisableSupport(*server_);

  // Client to server got lost (or arrives late)
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_->state(), State::OFF);
}

TEST_P(DtlsStunPiggybackControllerTest, NotSupportedByClient) {
  DisableSupport(*client_);

  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::OFF);
}

TEST_P(DtlsStunPiggybackControllerTest, SomeRequestsDoNotGoThrough) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 1+2, server sent request got lost.
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK
  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::COMPLETE);
}

TEST_P(DtlsStunPiggybackControllerTest, LossOnPostHandshakeAck) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK. Client to server gets lost
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::COMPLETE);
}

TEST_P(DtlsStunPiggybackControllerTest,
       UnsupportedStateAfterFallbackHandshakeRemainsOff) {
  DisableSupport(*client_);
  DisableSupport(*server_);

  // Set DTLS complete after normal handshake.
  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
  EXPECT_EQ(client_->state(), State::OFF);
  server_->SetDtlsHandshakeComplete(/*is_client=*/false, /*is_dtls13=*/false);
  EXPECT_EQ(server_->state(), State::OFF);
}

TEST_P(DtlsStunPiggybackControllerTest, DtlsFailed) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  server_->CapturePacket(dtls_flight2);
  server_->Flush();
  ASSERT_EQ(server_->state(), State::CONFIRMED);
  ASSERT_THAT(server_->GetPending(), SizeIs(1));

  EXPECT_CALL(*this, ServerCompleteCallback(false));
  server_->SetDtlsFailed();
  EXPECT_EQ(server_->state(), State::OFF);
  EXPECT_THAT(server_->GetPending(), IsEmpty());
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
}

TEST_P(DtlsStunPiggybackControllerTest, BasicHandshakeAckData) {
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));
  EXPECT_EQ(client_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));

  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  EXPECT_THAT(*client_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight2)}));

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
  EXPECT_THAT(*client_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight2),
                  ComputeDtlsPacketHash(dtls_flight4),
              }));

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback);
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ServerCompleteCallback);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::COMPLETE);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_->GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_P(DtlsStunPiggybackControllerTest, UnwrappedHandshakeAckData) {
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));
  EXPECT_EQ(client_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));

  // Flight 1+2 (embedded)
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  EXPECT_THAT(*client_->GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight2)}));

  // Flight 3+4 (not embedded)
  SendClientToServerDtls(dtls_flight3);
  SendServerToClientDtls(dtls_flight4);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
  EXPECT_THAT(*client_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight2),
                  ComputeDtlsPacketHash(dtls_flight4),
              }));

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback);
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_CALL(*this, ServerCompleteCallback);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::COMPLETE);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_->GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_P(DtlsStunPiggybackControllerTest, AckDataNoDuplicates) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));

  // Receive Flight 1 again, no change expected.
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
}

TEST_P(DtlsStunPiggybackControllerTest, AckDataNoDuplicatesFromDualReporting) {
  std::unique_ptr<StunByteStringAttribute> attr_data =
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1);
  std::unique_ptr<StunByteStringAttribute> attr_ack;
  if (auto ack = client_->GetAckToPiggyback(STUN_BINDING_REQUEST)) {
    attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
  }
  ASSERT_THAT(attr_ack, NotNull());
  server_->ReportDataPiggybacked(attr_data->array_view(),
                                 FromAckAttribute(attr_ack->array_view()));
  server_->ReportDtlsPacket(dtls_flight1);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
}

TEST_P(DtlsStunPiggybackControllerTest, IgnoresNonDtlsData) {
  std::vector<uint8_t> ascii = {0x64, 0x72, 0x6f, 0x70, 0x6d, 0x65};

  EXPECT_CALL(*this, ServerPacketSink).Times(0);
  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, ascii)->array_view(),
      std::nullopt);
  EXPECT_EQ(0, server_->GetCountOfReceivedData());
}

TEST_P(DtlsStunPiggybackControllerTest, DontSendAckedPackets) {
  server_->CapturePacket(dtls_flight1);
  server_->Flush();
  EXPECT_TRUE(server_->GetDataToPiggyback(STUN_BINDING_REQUEST).has_value());
  server_->ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1)}));
  // No unacked packet exists, i.e. empty response.
  auto response = server_->GetDataToPiggyback(STUN_BINDING_REQUEST);
  EXPECT_TRUE(response && response->empty());
}

TEST_P(DtlsStunPiggybackControllerTest, LimitAckSize) {
  std::vector<uint8_t> dtls_flight5 = FakeDtlsPacket(0x5487);

  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 1u);
  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight2)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 2u);
  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight3)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 3u);
  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight4)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 4u);

  // Limit size of ack so that it does not grow unbounded.
  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight5)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(),
            DtlsStunPiggybackController::kMaxAckSize);
  EXPECT_THAT(*server_->GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight2),
                  ComputeDtlsPacketHash(dtls_flight3),
                  ComputeDtlsPacketHash(dtls_flight4),
                  ComputeDtlsPacketHash(dtls_flight5),
              }));
}

TEST_P(DtlsStunPiggybackControllerTest, EmptyDataDoesNotClearAck) {
  std::vector<uint8_t> dtls_flight5 = FakeDtlsPacket(0x5487);

  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 1u);

  // The fact that we don't get any data does not mean that
  // we can clear the ack list.
  // a) packets can be arbitrary reordered.
  // b) the peer might be needing 2 packets (ie. pqc) to produce
  // a return packet and only one of them has arrived.
  server_->ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1)}));

  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 1u);
}

TEST_P(DtlsStunPiggybackControllerTest, NoEmptyDataInPending) {
  std::vector<uint8_t> packet = FakeDtlsPacket(0x5487);

  server_->ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, packet)->array_view(),
      std::nullopt);
  // If this is one of the two packets of a PQC client hello the server
  // does not have a response yet.
  auto response = server_->GetDataToPiggyback(STUN_BINDING_REQUEST);
  EXPECT_TRUE(response && response->empty());
  EXPECT_EQ(server_->GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 1u);
}

TEST_P(DtlsStunPiggybackControllerTest, MultiPacketRoundRobin) {
  // Let's pretend that a flight is 3 packets...
  server_->CapturePacket(dtls_flight1);
  server_->CapturePacket(dtls_flight2);
  server_->CapturePacket(dtls_flight3);
  server_->Flush();
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight1.begin(), dtls_flight1.end()));
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight3.begin(), dtls_flight3.end()));

  server_->ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1)}));

  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight3.begin(), dtls_flight3.end()));

  server_->ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight3)}));

  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_->GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
}

TEST_P(DtlsStunPiggybackControllerTest, DuplicateAck) {
  server_->CapturePacket(dtls_flight1);
  server_->Flush();
  server_->ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1),
                             ComputeDtlsPacketHash(dtls_flight1)}));
}

// In DTLS 1.3 the last flight is sent by the client, so the roles in the
// termination are swapped compared to DTLS 1.2. The server is done once it
// has sent its own flight and must keep retransmitting it until acked.
TEST_P(DtlsStunPiggybackControllerTest,
       Dtls13KeepsPendingPacketsOnHandshakeComplete) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  server_->CapturePacket(dtls_flight2);
  server_->Flush();
  ASSERT_THAT(server_->GetPending(), SizeIs(1));

  server_->SetDtlsHandshakeComplete(/*is_client=*/false, /*is_dtls13=*/true);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_THAT(server_->GetPending(), SizeIs(1));

  // Same for the client, whose flight 3 is the last one of the handshake.
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  client_->CapturePacket(dtls_flight3);
  client_->Flush();
  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/true);
  EXPECT_EQ(client_->state(), State::PENDING);
  EXPECT_THAT(client_->GetPending(), SizeIs(1));
}

TEST_F(DtlsStunPiggybackControllerSpedTest, Dtls13Handshake) {
  // Flight 1+2. The 1.3 server can send application data once it has sent its
  // own flight, i.e. before it has seen the client Finished.
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  server_->SetDtlsHandshakeComplete(/*is_client=*/false, /*is_dtls13=*/true);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3, the last one, acks flight 2 and empties the server stash.
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/true);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Closing handshake, ack-only in this direction.
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::COMPLETE);

  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerGoogSpedTest, Dtls13Handshake) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  server_->SetDtlsHandshakeComplete(/*is_client=*/false, /*is_dtls13=*/true);
  EXPECT_EQ(server_->state(), State::PENDING);

  // The ack for flight 2 arrives with flight 3, so the server empties its
  // stash and completes inside the same call that processes the last flight.
  EXPECT_CALL(*this, ServerCompleteCallback(true));
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/true);
  EXPECT_EQ(server_->state(), State::COMPLETE);
  EXPECT_EQ(client_->state(), State::PENDING);

  // A COMPLETE peer sends neither attribute, so the ack for flight 3 never
  // goes on the wire and the client keeps retransmitting it.
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::PENDING);
  EXPECT_THAT(client_->GetPending(), SizeIs(1));

  // Only an application packet unblocks the client.
  EXPECT_CALL(*this, ClientCompleteCallback(true));
  client_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kDtlsDecrypted));
  EXPECT_EQ(client_->state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerGoogSpedTest,
       BasicHandshakeCompleteWithDecryptedPacket) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback);
  client_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kDtlsDecrypted));
  EXPECT_EQ(client_->state(), State::COMPLETE);

  EXPECT_CALL(*this, ServerCompleteCallback);
  server_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kSrtpEncrypted));
  EXPECT_EQ(server_->state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerGoogSpedTest,
       BasicHandshakeEarlySrtpDoesNotComplete) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_->state(), State::CONFIRMED);

  // Flight 3
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_->state(), State::CONFIRMED);

  // An srtp packet arriving before reaching PENDING state.
  server_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kSrtpEncrypted));
  EXPECT_EQ(server_->state(), State::CONFIRMED);

  // Flight 4
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_->state(), State::PENDING);
  EXPECT_EQ(client_->state(), State::PENDING);

  // Post-handshake ACK
  EXPECT_CALL(*this, ClientCompleteCallback);
  client_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kDtlsDecrypted));
  EXPECT_EQ(client_->state(), State::COMPLETE);

  EXPECT_CALL(*this, ServerCompleteCallback);
  server_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kSrtpEncrypted));
  EXPECT_EQ(server_->state(), State::COMPLETE);
}

// Application packets are not a completion signal here, the closing handshake
// is.
TEST_F(DtlsStunPiggybackControllerSpedTest,
       ApplicationPacketDoesNotCompleteHandshake) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  ASSERT_EQ(server_->state(), State::PENDING);
  ASSERT_EQ(client_->state(), State::PENDING);

  EXPECT_CALL(*this, ClientCompleteCallback).Times(0);
  EXPECT_CALL(*this, ServerCompleteCallback).Times(0);
  client_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kDtlsDecrypted));
  server_->ApplicationPacketReceived(
      packet_.CopyAndSet(ReceivedIpPacket::kSrtpEncrypted));
  EXPECT_EQ(client_->state(), State::PENDING);
  EXPECT_EQ(server_->state(), State::PENDING);
}

// The DTLS 1.2 client is complete when it receives flight 4, which the server
// only sends after flight 3 arrived. Its stash can therefore be dropped.
TEST_F(DtlsStunPiggybackControllerSpedTest,
       Dtls12ClientClearsPendingPacketsOnHandshakeComplete) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  client_->CapturePacket(dtls_flight3);
  client_->Flush();
  ASSERT_THAT(client_->GetPending(), SizeIs(1));

  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
  EXPECT_EQ(client_->state(), State::PENDING);
  EXPECT_THAT(client_->GetPending(), IsEmpty());
  EXPECT_EQ(client_->GetDataToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

// The default variant keeps retransmitting flight 3 instead, since an empty
// stash is what terminates the session there.
TEST_F(DtlsStunPiggybackControllerGoogSpedTest,
       Dtls12ClientKeepsPendingPacketsOnHandshakeComplete) {
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  client_->CapturePacket(dtls_flight3);
  client_->Flush();
  ASSERT_THAT(client_->GetPending(), SizeIs(1));

  client_->SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
  EXPECT_EQ(client_->state(), State::PENDING);
  EXPECT_THAT(client_->GetPending(), SizeIs(1));
}

}  // namespace webrtc
