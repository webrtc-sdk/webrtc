/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller_sped.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/strings/str_join.h"

namespace webrtc {

DtlsStunPiggybackControllerSped::DtlsStunPiggybackControllerSped(
    absl::AnyInvocable<void(std::span<const uint8_t>)> dtls_data_callback,
    // NOLINTNEXTLINE(readability/casting) - not a cast; false positive!
    absl::AnyInvocable<void(bool) &&> piggyback_complete_callback)
    : dtls_data_callback_(std::move(dtls_data_callback)),
      piggyback_complete_callback_(std::move(piggyback_complete_callback)) {}

DtlsStunPiggybackControllerSped::~DtlsStunPiggybackControllerSped() {
  RTC_DCHECK(dtls_data_callback_);
  RTC_DCHECK(piggyback_complete_callback_);
}

void DtlsStunPiggybackControllerSped::SetDtlsHandshakeComplete(
    bool is_dtls_client,
    bool is_dtls13) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Peer does not support this so fallback to a normal DTLS handshake
  // happened.
  if (state_ == State::OFF) {
    return;
  }

  // As DTLS 1.2 client we have nothing more to send at this point
  // but will continue to send ACK attributes until receiving
  // the last flight from the server.
  if (is_dtls_client && !is_dtls13) {
    pending_packets_.clear();
  }
  state_ = State::PENDING;
}

void DtlsStunPiggybackControllerSped::ApplicationPacketReceived(
    const ReceivedIpPacket& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // TODO: bugs.webrtc.org/367395350 - remove this.
}

void DtlsStunPiggybackControllerSped::SetDtlsFailed() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::TENTATIVE || state_ == State::CONFIRMED ||
      state_ == State::PENDING) {
    RTC_LOG(LS_INFO)
        << "DTLS-STUN piggybacking DTLS failed during negotiation.";
  }
  state_ = State::OFF;
  CallCompleteCallback(/*success=*/false);
}

void DtlsStunPiggybackControllerSped::CapturePacket(
    std::span<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!IsDtlsPacket(data)) {
    return;
  }

  // BoringSSL writes burst of packets...but the interface
  // is made for 1-packet at a time. Use the writing_packets_ variable to keep
  // track of a full flight. The writing_packets_ is reset in Flush.
  if (!writing_packets_) {
    pending_packets_.clear();
    writing_packets_ = true;
  }

  pending_packets_.Add(data);
}

void DtlsStunPiggybackControllerSped::ClearCachedPacketForTesting() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  pending_packets_.clear();
}

void DtlsStunPiggybackControllerSped::Flush() {
  // Flush is called by the StreamInterface (and the underlying SSL BIO)
  // after a flight of packets has been sent.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  writing_packets_ = false;
}

std::optional<absl::string_view>
DtlsStunPiggybackControllerSped::GetDataToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(stun_message_type == STUN_BINDING_REQUEST ||
             stun_message_type == STUN_BINDING_RESPONSE);

  if (state_ == State::COMPLETE) {
    return std::nullopt;
  }

  if (state_ == State::OFF) {
    return std::nullopt;
  }

  // No longer writing packets...since we're now about to send them.
  RTC_DCHECK(!writing_packets_);

  if (pending_packets_.empty()) {
    // In confirmed state include an empty data attribute. Can happen e.g.
    // with PQC after receiving a partial flight.
    // In unconfirmed and pending states do not include the attribute.
    if (state_ == State::CONFIRMED) {
      return "";
    }
    return std::nullopt;
  }

  const auto packet = pending_packets_.GetNext();
  return absl::string_view(reinterpret_cast<const char*>(packet.data()),
                           packet.size());
}

std::optional<const std::vector<uint32_t>>
DtlsStunPiggybackControllerSped::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  return handshake_messages_received_;
}

std::vector<std::span<const uint8_t>>
DtlsStunPiggybackControllerSped::GetPending() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return pending_packets_.GetAll();
}

void DtlsStunPiggybackControllerSped::ReportDataPiggybacked(
    std::optional<std::span<uint8_t>> data,
    std::optional<std::vector<uint32_t>> acks) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Drop silently when receiving acked data when the peer previously did not
  // support or we already moved to the complete state.
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }

  // We sent dtls piggybacked but got nothing in return or
  // we received a stun request with neither attribute set
  // => peer does not support.
  if (state_ == State::TENTATIVE && !data.has_value() && !acks.has_value()) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking not supported by peer.";
    state_ = State::OFF;
    // TODO: bugs.webrtc.org/367395350 - we cached a client hello
    // which needs to be sent by the DTLS transport now.
    // CallCompleteCallback(/*success=*/false);
    return;
  }

  // In PENDING state the peer may have stopped sending the ack
  // when it moved to the COMPLETE state. Move to the same state.
  if (state_ == State::PENDING && !data.has_value() && !acks.has_value()) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    CallCompleteCallback(/*success=*/true);
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  if (acks.has_value()) {
    if (!pending_packets_.empty()) {
      // Unpack the ACK attribute (a list of uint32_t)
      absl::flat_hash_set<uint32_t> acked_packets;
      for (const auto& ack : *acks) {
        acked_packets.insert(ack);
      }
      RTC_LOG(LS_VERBOSE) << "DTLS-STUN piggybacking ACK: "
                          << StrJoin(acked_packets, ",");

      // Remove all acked packets from pending_packets_.
      pending_packets_.Prune(acked_packets);
    }
  }

  // The response to the final flight of the handshake will not contain
  // the DTLS data but will contain an ack.
  // Must not happen on the initial server to client packet which
  // has no DTLS data yet.
  if (state_ == State::PENDING && !data.has_value() && acks.has_value()) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    CallCompleteCallback(/*success=*/true);
    return;
  }

  if (!data.has_value() || data->empty()) {
    return;
  }
  // Drop non-DTLS packets.
  if (!IsDtlsPacket(*data)) {
    RTC_LOG(LS_WARNING) << "Dropping non-DTLS data.";
    return;
  }
  data_recv_count_++;
  ReportDtlsPacket(*data);

  // Forwards the data to the DTLS layer. Note that this will call
  // ProcessDtlsPacket() again which does not change the state.
  dtls_data_callback_(*data);
}

void DtlsStunPiggybackControllerSped::ReportDtlsPacket(
    std::span<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }

  // Extract the received message id of the handshake
  // from the packet and prepare the ack to be sent.
  uint32_t hash = ComputeDtlsPacketHash(data);

  // Check if we already received this packet.
  if (std::find(handshake_messages_received_.begin(),
                handshake_messages_received_.end(),
                hash) == handshake_messages_received_.end()) {
    // If needed, limit size of ack attribute by removing oldest ack.
    while (handshake_messages_received_.size() >= kMaxAckSize) {
      handshake_messages_received_.erase(handshake_messages_received_.begin());
    }
    handshake_messages_received_.push_back(hash);
  }
}

void DtlsStunPiggybackControllerSped::CallCompleteCallback(bool success) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  pending_packets_.clear();
  handshake_messages_received_.clear();
  if (!piggyback_complete_callback_) {
    RTC_DCHECK_NOTREACHED() << "CompleteCallback called twice!";
    return;
  }
  std::move(piggyback_complete_callback_)(success);
}

}  // namespace webrtc
