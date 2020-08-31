// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_TLS_CHLO_EXTRACTOR_H_
#define QUICHE_QUIC_CORE_TLS_CHLO_EXTRACTOR_H_

#include <memory>
#include <string>
#include <vector>
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "net/third_party/quiche/src/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_stream_sequencer.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"

namespace quic {

// Utility class that allows extracting information from a QUIC-TLS Client
// Hello. This class creates a QuicFramer to parse the packet, and implements
// QuicFramerVisitorInterface to access the frames parsed by the QuicFramer. It
// then uses a QuicStreamSequencer to reassemble the contents of the crypto
// stream, and implements QuicStreamSequencer::StreamInterface to access the
// reassembled data.
class QUIC_NO_EXPORT TlsChloExtractor
    : public QuicFramerVisitorInterface,
      public QuicStreamSequencer::StreamInterface {
 public:
  TlsChloExtractor();
  TlsChloExtractor(const TlsChloExtractor&) = delete;
  TlsChloExtractor(TlsChloExtractor&&);
  TlsChloExtractor& operator=(const TlsChloExtractor&) = delete;
  TlsChloExtractor& operator=(TlsChloExtractor&&);

  enum class State : uint8_t {
    kInitial = 0,
    kParsedFullSinglePacketChlo = 1,
    kParsedFullMultiPacketChlo = 2,
    kParsedPartialChloFragment = 3,
    kUnrecoverableFailure = 4,
  };

  State state() const { return state_; }
  std::vector<std::string> alpns() const { return alpns_; }
  std::string server_name() const { return server_name_; }

  // Converts |state| to a human-readable string suitable for logging.
  static std::string StateToString(State state);

  // Ingests |packet| and attempts to parse out the CHLO.
  void IngestPacket(const ParsedQuicVersion& version,
                    const QuicReceivedPacket& packet);

  // Returns whether the ingested packets have allowed parsing a complete CHLO.
  bool HasParsedFullChlo() const {
    return state_ == State::kParsedFullSinglePacketChlo ||
           state_ == State::kParsedFullMultiPacketChlo;
  }

  // Methods from QuicFramerVisitorInterface.
  void OnError(QuicFramer* /*framer*/) override {}
  bool OnProtocolVersionMismatch(ParsedQuicVersion version) override;
  void OnPacket() override {}
  void OnPublicResetPacket(const QuicPublicResetPacket& /*packet*/) override {}
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& /*packet*/) override {}
  void OnRetryPacket(QuicConnectionId /*original_connection_id*/,
                     QuicConnectionId /*new_connection_id*/,
                     quiche::QuicheStringPiece /*retry_token*/,
                     quiche::QuicheStringPiece /*retry_integrity_tag*/,
                     quiche::QuicheStringPiece /*retry_without_tag*/) override {
  }
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override;
  bool OnUnauthenticatedHeader(const QuicPacketHeader& /*header*/) override {
    return true;
  }
  void OnDecryptedPacket(EncryptionLevel /*level*/) override {}
  bool OnPacketHeader(const QuicPacketHeader& /*header*/) override {
    return true;
  }
  void OnCoalescedPacket(const QuicEncryptedPacket& /*packet*/) override {}
  void OnUndecryptablePacket(const QuicEncryptedPacket& /*packet*/,
                             EncryptionLevel /*decryption_level*/,
                             bool /*has_decryption_key*/) override {}
  bool OnStreamFrame(const QuicStreamFrame& /*frame*/) override { return true; }
  bool OnCryptoFrame(const QuicCryptoFrame& frame) override;
  bool OnAckFrameStart(QuicPacketNumber /*largest_acked*/,
                       QuicTime::Delta /*ack_delay_time*/) override {
    return true;
  }
  bool OnAckRange(QuicPacketNumber /*start*/,
                  QuicPacketNumber /*end*/) override {
    return true;
  }
  bool OnAckTimestamp(QuicPacketNumber /*packet_number*/,
                      QuicTime /*timestamp*/) override {
    return true;
  }
  bool OnAckFrameEnd(QuicPacketNumber /*start*/) override { return true; }
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& /*frame*/) override {
    return true;
  }
  bool OnPingFrame(const QuicPingFrame& /*frame*/) override { return true; }
  bool OnRstStreamFrame(const QuicRstStreamFrame& /*frame*/) override {
    return true;
  }
  bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& /*frame*/) override {
    return true;
  }
  bool OnNewConnectionIdFrame(
      const QuicNewConnectionIdFrame& /*frame*/) override {
    return true;
  }
  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& /*frame*/) override {
    return true;
  }
  bool OnNewTokenFrame(const QuicNewTokenFrame& /*frame*/) override {
    return true;
  }
  bool OnStopSendingFrame(const QuicStopSendingFrame& /*frame*/) override {
    return true;
  }
  bool OnPathChallengeFrame(const QuicPathChallengeFrame& /*frame*/) override {
    return true;
  }
  bool OnPathResponseFrame(const QuicPathResponseFrame& /*frame*/) override {
    return true;
  }
  bool OnGoAwayFrame(const QuicGoAwayFrame& /*frame*/) override { return true; }
  bool OnMaxStreamsFrame(const QuicMaxStreamsFrame& /*frame*/) override {
    return true;
  }
  bool OnStreamsBlockedFrame(
      const QuicStreamsBlockedFrame& /*frame*/) override {
    return true;
  }
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& /*frame*/) override {
    return true;
  }
  bool OnBlockedFrame(const QuicBlockedFrame& /*frame*/) override {
    return true;
  }
  bool OnPaddingFrame(const QuicPaddingFrame& /*frame*/) override {
    return true;
  }
  bool OnMessageFrame(const QuicMessageFrame& /*frame*/) override {
    return true;
  }
  bool OnHandshakeDoneFrame(const QuicHandshakeDoneFrame& /*frame*/) override {
    return true;
  }
  void OnPacketComplete() override {}
  bool IsValidStatelessResetToken(QuicUint128 /*token*/) const override {
    return true;
  }
  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& /*packet*/) override {}

  // Methods from QuicStreamSequencer::StreamInterface.
  void OnDataAvailable() override;
  void OnFinRead() override {}
  void AddBytesConsumed(QuicByteCount /*bytes*/) override {}
  void Reset(QuicRstStreamErrorCode /*error*/) override {}
  void OnUnrecoverableError(QuicErrorCode error,
                            const std::string& details) override;
  QuicStreamId id() const override { return 0; }

 private:
  // Parses the length of the CHLO message by looking at the first four bytes.
  // Returns whether we have received enough data to parse the full CHLO now.
  bool MaybeAttemptToParseChloLength();
  // Parses the full CHLO message if enough data has been received.
  void AttemptToParseFullChlo();
  // Moves to the failed state and records the error details.
  void HandleUnrecoverableError(const std::string& error_details);
  // Lazily sets up shared SSL handles if needed.
  static std::pair<SSL_CTX*, int> GetSharedSslHandles();
  // Lazily sets up the per-instance SSL handle if needed.
  void SetupSslHandle();
  // Extract the TlsChloExtractor instance from |ssl|.
  static TlsChloExtractor* GetInstanceFromSSL(SSL* ssl);

  // BoringSSL static TLS callbacks.
  static enum ssl_select_cert_result_t SelectCertCallback(
      const SSL_CLIENT_HELLO* client_hello);
  static int SetReadSecretCallback(SSL* ssl,
                                   enum ssl_encryption_level_t level,
                                   const SSL_CIPHER* cipher,
                                   const uint8_t* secret,
                                   size_t secret_length);
  static int SetWriteSecretCallback(SSL* ssl,
                                    enum ssl_encryption_level_t level,
                                    const SSL_CIPHER* cipher,
                                    const uint8_t* secret,
                                    size_t secret_length);
  static int WriteMessageCallback(SSL* ssl,
                                  enum ssl_encryption_level_t level,
                                  const uint8_t* data,
                                  size_t len);
  static int FlushFlightCallback(SSL* ssl);
  static int SendAlertCallback(SSL* ssl,
                               enum ssl_encryption_level_t level,
                               uint8_t desc);

  // Called by SelectCertCallback.
  void HandleParsedChlo(const SSL_CLIENT_HELLO* client_hello);
  // Called by callbacks that should never be called.
  void HandleUnexpectedCallback(const std::string& callback_name);
  // Called by SendAlertCallback.
  void SendAlert(uint8_t tls_alert_value);

  // Used to parse received packets to extract single frames.
  std::unique_ptr<QuicFramer> framer_;
  // Used to reassemble the crypto stream from received CRYPTO frames.
  QuicStreamSequencer crypto_stream_sequencer_;
  // BoringSSL handle required to parse the CHLO.
  bssl::UniquePtr<SSL> ssl_;
  // State of this TlsChloExtractor.
  State state_;
  // Detail string that can be logged in the presence of unrecoverable errors.
  std::string error_details_;
  // Whether a CRYPTO frame was parsed in this packet.
  bool parsed_crypto_frame_in_this_packet_;
  // Array of ALPNs parsed from the CHLO.
  std::vector<std::string> alpns_;
  // SNI parsed from the CHLO.
  std::string server_name_;
};

// Convenience method to facilitate logging TlsChloExtractor::State.
QUIC_NO_EXPORT std::ostream& operator<<(std::ostream& os,
                                        const TlsChloExtractor::State& state);

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_TLS_CHLO_EXTRACTOR_H_
