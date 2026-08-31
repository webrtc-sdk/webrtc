/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_INTERFACE_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_INTERFACE_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/transport/stun.h"
#include "rtc_base/network/received_packet.h"

namespace webrtc {

// Abstract interface for piggybacking DTLS handshake packets in STUN
// connectivity checks. Implementations are not thread safe; all methods must
// be called on the same thread as the constructor.
class DtlsStunPiggybackControllerInterface {
 public:
  enum class State {
    // We don't know if peer support DTLS piggybacked in STUN.
    // We will piggyback DTLS until we get a piggybacked response
    // or a STUN response with piggyback support.
    TENTATIVE = 0,
    // The peer supports DTLS in STUN and we continue the handshake.
    CONFIRMED = 1,
    // We are waiting for the final ack. Semantic differs depending
    // on DTLS role.
    PENDING = 2,
    // We successfully completed the DTLS handshake in STUN.
    COMPLETE = 3,
    // The peer does not support piggybacking DTLS in STUN.
    OFF = 4,
  };

  virtual ~DtlsStunPiggybackControllerInterface() = default;

  virtual State state() const = 0;

  virtual void SetDtlsHandshakeComplete(bool is_dtls_client,
                                        bool is_dtls13) = 0;
  virtual void ApplicationPacketReceived(const ReceivedIpPacket& packet) = 0;
  virtual void SetDtlsFailed() = 0;

  virtual void CapturePacket(std::span<const uint8_t> data) = 0;
  virtual void ClearCachedPacketForTesting() = 0;

  virtual void Flush() = 0;

  virtual std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type) = 0;
  virtual std::optional<const std::vector<uint32_t>> GetAckToPiggyback(
      StunMessageType stun_message_type) = 0;
  virtual std::vector<std::span<const uint8_t>> GetPending() = 0;

  virtual void ReportDataPiggybacked(
      std::optional<std::span<uint8_t>> data,
      std::optional<std::vector<uint32_t>> acks) = 0;

  virtual void ReportDtlsPacket(std::span<const uint8_t> data) = 0;

  virtual int GetCountOfReceivedData() const = 0;
};

}  // namespace webrtc

#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_INTERFACE_H_
