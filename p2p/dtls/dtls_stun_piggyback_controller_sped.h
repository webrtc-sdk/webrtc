/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_SPED_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_SPED_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_stun_piggyback_controller_interface.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsStunPiggybackControllerSped
    : public DtlsStunPiggybackControllerInterface {
 public:
  // Never ack more than 4 packets.
  static constexpr unsigned kMaxAckSize = 4;

  // dtls_data_callback will be called with any DTLS packets received
  // piggybacked.
  DtlsStunPiggybackControllerSped(
      absl::AnyInvocable<void(std::span<const uint8_t>)> dtls_data_callback,
      // NOLINTNEXTLINE(readability/casting) - not a cast; false positive!
      absl::AnyInvocable<void(bool) &&> piggyback_complete_callback);

  ~DtlsStunPiggybackControllerSped() override;

  State state() const override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return state_;
  }

  // Called by DtlsTransport when the handshake is complete "locally",
  // i.e. we can send encrypted packets to peer (but we don't strictly know
  // that peer can decode them).
  void SetDtlsHandshakeComplete(bool is_dtls_client, bool is_dtls13) override;

  // Called by DtlsTransport when a packet has been received and passed
  // to layers above us. This means that dtls is writable for the peer,
  // and maybe we are complete.
  void ApplicationPacketReceived(const ReceivedIpPacket& packet) override;

  // Called by DtlsTransport when DTLS failed.
  void SetDtlsFailed() override;

  // Intercepts DTLS packets which should go into the STUN packets during the
  // handshake.
  void CapturePacket(std::span<const uint8_t> data) override;
  void ClearCachedPacketForTesting() override;

  // Inform piggybackcontroller that a flight is complete.
  void Flush() override;

  // Called by Connection, when sending a STUN BINDING { REQUEST / RESPONSE }
  // to obtain optional DTLS data or ACKs.
  std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type) override;
  std::optional<const std::vector<uint32_t>> GetAckToPiggyback(
      StunMessageType stun_message_type) override;
  std::vector<std::span<const uint8_t>> GetPending() override;

  // Called by Connection when receiving a STUN BINDING { REQUEST / RESPONSE }.
  void ReportDataPiggybacked(
      std::optional<std::span<uint8_t>> data,
      std::optional<std::vector<uint32_t>> acks) override;

  // Called by
  // * DTLSTransport when receiving a DTLS packet (possibly after the packet
  //   was emitted by this class).
  // * This class when processing a DTLS packet.
  void ReportDtlsPacket(std::span<const uint8_t> data) override;

  int GetCountOfReceivedData() const override { return data_recv_count_; }

 private:
  State state_ RTC_GUARDED_BY(sequence_checker_) = State::TENTATIVE;
  bool writing_packets_ RTC_GUARDED_BY(sequence_checker_) = false;
  PacketStash pending_packets_ RTC_GUARDED_BY(sequence_checker_);
  absl::AnyInvocable<void(std::span<const uint8_t>)> dtls_data_callback_
      RTC_GUARDED_BY(sequence_checker_);
  // NOLINTNEXTLINE(readability/casting) - not a cast; false positive!
  absl::AnyInvocable<void(bool) &&> piggyback_complete_callback_
      RTC_GUARDED_BY(sequence_checker_);

  std::vector<uint32_t> handshake_messages_received_
      RTC_GUARDED_BY(sequence_checker_);

  // Count of embedded data attributes received.
  int data_recv_count_ = 0;

  void CallCompleteCallback(bool success);

  // In practice this will be the network thread.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}  //  namespace webrtc

#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_SPED_H_
