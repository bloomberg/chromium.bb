// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_QUIC_TRANSPORT_QUIC_TRANSPORT_SESSION_H_
#define QUICHE_QUIC_QUIC_TRANSPORT_QUIC_TRANSPORT_SESSION_H_

#include <cstdint>
#include <memory>

#include "absl/strings/string_view.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "quic/core/crypto/quic_crypto_client_config.h"
#include "quic/core/quic_config.h"
#include "quic/core/quic_connection.h"
#include "quic/core/quic_crypto_client_stream.h"
#include "quic/core/quic_crypto_stream.h"
#include "quic/core/quic_datagram_queue.h"
#include "quic/core/quic_error_codes.h"
#include "quic/core/quic_server_id.h"
#include "quic/core/quic_session.h"
#include "quic/core/quic_stream.h"
#include "quic/core/quic_versions.h"
#include "quic/core/web_transport_interface.h"
#include "quic/platform/api/quic_bug_tracker.h"
#include "quic/platform/api/quic_containers.h"
#include "quic/quic_transport/quic_transport_protocol.h"
#include "quic/quic_transport/quic_transport_session_interface.h"
#include "quic/quic_transport/quic_transport_stream.h"

namespace quic {

// A client session for the QuicTransport protocol.
class QUIC_EXPORT_PRIVATE QuicTransportClientSession
    : public QuicSession,
      public WebTransportSession,
      public QuicTransportSessionInterface,
      public QuicCryptoClientStream::ProofHandler {
 public:
  QuicTransportClientSession(
      QuicConnection* connection,
      Visitor* owner,
      const QuicConfig& config,
      const ParsedQuicVersionVector& supported_versions,
      const GURL& url,
      QuicCryptoClientConfig* crypto_config,
      url::Origin origin,
      WebTransportVisitor* visitor,
      std::unique_ptr<QuicDatagramQueue::Observer> datagram_observer);

  std::vector<std::string> GetAlpnsToOffer() const override {
    return std::vector<std::string>({QuicTransportAlpn()});
  }
  void OnAlpnSelected(absl::string_view alpn) override;
  bool alpn_received() const { return alpn_received_; }

  void CryptoConnect() { crypto_stream_->CryptoConnect(); }

  bool ShouldKeepConnectionAlive() const override { return true; }

  QuicCryptoStream* GetMutableCryptoStream() override {
    return crypto_stream_.get();
  }
  const QuicCryptoStream* GetCryptoStream() const override {
    return crypto_stream_.get();
  }

  // Returns true once the encryption has been established and the client
  // indication has been sent.  No application data will be read or written
  // before the connection is ready.  Once the connection becomes ready, this
  // method will never return false.
  bool IsSessionReady() const override { return ready_; }

  QuicStream* CreateIncomingStream(QuicStreamId id) override;
  QuicStream* CreateIncomingStream(PendingStream* /*pending*/) override {
    QUIC_BUG(quic_bug_10890_1)
        << "QuicTransportClientSession::CreateIncomingStream("
           "PendingStream) not implemented";
    return nullptr;
  }

  void SetDefaultEncryptionLevel(EncryptionLevel level) override;
  void OnTlsHandshakeComplete() override;
  void OnMessageReceived(absl::string_view message) override;

  // Return the earliest incoming stream that has been received by the session
  // but has not been accepted.  Returns nullptr if there are no incoming
  // streams.
  QuicTransportStream* AcceptIncomingBidirectionalStream() override;
  QuicTransportStream* AcceptIncomingUnidirectionalStream() override;

  bool CanOpenNextOutgoingBidirectionalStream() override {
    return QuicSession::CanOpenNextOutgoingBidirectionalStream();
  }
  bool CanOpenNextOutgoingUnidirectionalStream() override {
    return QuicSession::CanOpenNextOutgoingUnidirectionalStream();
  }
  QuicTransportStream* OpenOutgoingBidirectionalStream() override;
  QuicTransportStream* OpenOutgoingUnidirectionalStream() override;

  MessageStatus SendOrQueueDatagram(QuicMemSlice datagram) override {
    return datagram_queue()->SendOrQueueDatagram(std::move(datagram));
  }
  void SetDatagramMaxTimeInQueue(QuicTime::Delta max_time_in_queue) override {
    datagram_queue()->SetMaxTimeInQueue(max_time_in_queue);
  }

  // For unit tests.
  using QuicSession::datagram_queue;

  // QuicCryptoClientStream::ProofHandler implementation.
  void OnProofValid(const QuicCryptoClientConfig::CachedState& cached) override;
  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) override;

  void CloseSession(WebTransportSessionError /*error_code*/,
                    absl::string_view error_message) override {
    connection()->CloseConnection(
        QUIC_NO_ERROR, std::string(error_message),
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }

  QuicByteCount GetMaxDatagramSize() const override {
    return GetGuaranteedLargestMessagePayload();
  }

 protected:
  class QUIC_EXPORT_PRIVATE ClientIndication : public QuicStream {
   public:
    using QuicStream::QuicStream;

    // This method should never be called, since the stream is client-initiated
    // unidirectional.
    void OnDataAvailable() override {
      QUIC_BUG(quic_bug_10890_2) << "Received data on a write-only stream";
    }
  };

  // Creates and activates a QuicTransportStream for the given ID.
  QuicTransportStream* CreateStream(QuicStreamId id);

  // Serializes the client indication as described in
  // https://vasilvv.github.io/webtransport/draft-vvv-webtransport-quic.html#rfc.section.3.2
  std::string SerializeClientIndication();
  // Creates the client indication stream and sends the client indication on it.
  void SendClientIndication();

  void OnCanCreateNewOutgoingStream(bool unidirectional) override;

  std::unique_ptr<QuicCryptoClientStream> crypto_stream_;
  GURL url_;
  url::Origin origin_;
  WebTransportVisitor* visitor_;  // not owned
  bool client_indication_sent_ = false;
  bool alpn_received_ = false;
  bool ready_ = false;

  // Contains all of the streams that has been received by the session but have
  // not been processed by the application.
  // TODO(vasilvv): currently, we always send MAX_STREAMS as long as the overall
  // maximum number of streams for the connection has not been exceeded. We
  // should also limit the maximum number of streams that the consuming code
  // has not accepted to a smaller number, by checking the size of
  // |incoming_bidirectional_streams_| and |incoming_unidirectional_streams_|
  // before sending MAX_STREAMS.
  quiche::QuicheCircularDeque<QuicTransportStream*>
      incoming_bidirectional_streams_;
  quiche::QuicheCircularDeque<QuicTransportStream*>
      incoming_unidirectional_streams_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_QUIC_TRANSPORT_QUIC_TRANSPORT_SESSION_H_
