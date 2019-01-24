// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_session.h"

#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"

namespace quic {

namespace {

// Arbitrary server port number for net::QuicCryptoClientConfig.
const int kQuicServerPort = 0;

// Length of HKDF input keying material, equal to its number of bytes.
// https://tools.ietf.org/html/rfc5869#section-2.2.
// TODO(zhihuang): Verify that input keying material length is correct.
const size_t kInputKeyingMaterialLength = 32;

// Used by QuicCryptoServerConfig to provide dummy proof credentials.
// TODO(zhihuang): Remove when secure P2P QUIC handshake is possible.
class DummyProofSource : public ProofSource {
 public:
  DummyProofSource() {}
  ~DummyProofSource() override {}

  // ProofSource override.
  void GetProof(const QuicSocketAddress& server_address,
                const QuicString& hostname,
                const QuicString& server_config,
                QuicTransportVersion transport_version,
                QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override {
    QuicReferenceCountedPointer<ProofSource::Chain> chain =
        GetCertChain(server_address, hostname);
    QuicCryptoProof proof;
    proof.signature = "Dummy signature";
    proof.leaf_cert_scts = "Dummy timestamp";
    callback->Run(true, chain, proof, nullptr /* details */);
  }

  QuicReferenceCountedPointer<Chain> GetCertChain(
      const QuicSocketAddress& server_address,
      const QuicString& hostname) override {
    std::vector<QuicString> certs;
    certs.push_back("Dummy cert");
    return QuicReferenceCountedPointer<ProofSource::Chain>(
        new ProofSource::Chain(certs));
  }

  void ComputeTlsSignature(
      const QuicSocketAddress& server_address,
      const QuicString& hostname,
      uint16_t signature_algorithm,
      QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override {
    callback->Run(true, "Dummy signature");
  }
};

// Used by QuicCryptoClientConfig to ignore the peer's credentials
// and establish an insecure QUIC connection.
// TODO(zhihuang): Remove when secure P2P QUIC handshake is possible.
class InsecureProofVerifier : public ProofVerifier {
 public:
  InsecureProofVerifier() {}
  ~InsecureProofVerifier() override {}

  // ProofVerifier override.
  QuicAsyncStatus VerifyProof(
      const QuicString& hostname,
      const uint16_t port,
      const QuicString& server_config,
      QuicTransportVersion transport_version,
      QuicStringPiece chlo_hash,
      const std::vector<QuicString>& certs,
      const QuicString& cert_sct,
      const QuicString& signature,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* verify_details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return QUIC_SUCCESS;
  }

  QuicAsyncStatus VerifyCertChain(
      const QuicString& hostname,
      const std::vector<QuicString>& certs,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return QUIC_SUCCESS;
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

}  // namespace

QuicConnectionId QuartcCryptoServerStreamHelper::GenerateConnectionIdForReject(
    QuicConnectionId connection_id) const {
  return EmptyQuicConnectionId();
}

bool QuartcCryptoServerStreamHelper::CanAcceptClientHello(
    const CryptoHandshakeMessage& message,
    const QuicSocketAddress& client_address,
    const QuicSocketAddress& peer_address,
    const QuicSocketAddress& self_address,
    QuicString* error_details) const {
  return true;
}

QuartcSession::QuartcSession(std::unique_ptr<QuicConnection> connection,
                             const QuicConfig& config,
                             const ParsedQuicVersionVector& supported_versions,
                             const QuicString& unique_remote_server_id,
                             Perspective perspective,
                             QuicConnectionHelperInterface* helper,
                             const QuicClock* clock,
                             std::unique_ptr<QuartcPacketWriter> packet_writer)
    : QuicSession(connection.get(),
                  nullptr /*visitor*/,
                  config,
                  supported_versions),
      unique_remote_server_id_(unique_remote_server_id),
      perspective_(perspective),
      packet_writer_(std::move(packet_writer)),
      connection_(std::move(connection)),
      helper_(helper),
      clock_(clock) {
  packet_writer_->set_connection(connection_.get());

  // Initialization with default crypto configuration.
  if (perspective_ == Perspective::IS_CLIENT) {
    std::unique_ptr<ProofVerifier> proof_verifier(new InsecureProofVerifier);
    quic_crypto_client_config_ = QuicMakeUnique<QuicCryptoClientConfig>(
        std::move(proof_verifier), TlsClientHandshaker::CreateSslCtx());
    quic_crypto_client_config_->set_pad_inchoate_hello(false);
    quic_crypto_client_config_->set_pad_full_hello(false);
  } else {
    std::unique_ptr<ProofSource> proof_source(new DummyProofSource);
    // Generate a random source address token secret. For long-running servers
    // it's better to not regenerate it for each connection to enable zero-RTT
    // handshakes, but for transient clients it does not matter.
    char source_address_token_secret[kInputKeyingMaterialLength];
    helper_->GetRandomGenerator()->RandBytes(source_address_token_secret,
                                             kInputKeyingMaterialLength);
    quic_crypto_server_config_ = QuicMakeUnique<QuicCryptoServerConfig>(
        QuicString(source_address_token_secret, kInputKeyingMaterialLength),
        helper_->GetRandomGenerator(), std::move(proof_source),
        KeyExchangeSource::Default(), TlsServerHandshaker::CreateSslCtx());

    // Effectively disables the anti-amplification measures (we don't need
    // them because we use ICE, and we need to disable them because we disable
    // padding of crypto packets).
    // This multiplier must be large enough so that the crypto handshake packet
    // (approx. 300 bytes) multiplied by this multiplier is larger than a fully
    // sized packet (currently 1200 bytes).
    // 1500 is a bit extreme: if you can imagine sending a 1 byte packet, and
    // your largest MTU would be below 1500 bytes, 1500*1 >=
    // any_packet_that_you_can_imagine_sending.
    // (again, we hardcode packet size to 1200, so we are not dealing with jumbo
    // frames).
    quic_crypto_server_config_->set_chlo_multiplier(1500);

    // We are sending small client hello, we must not validate its size.
    quic_crypto_server_config_->set_validate_chlo_size(false);

    // We run QUIC over ICE, and ICE is verifying remote side with STUN pings.
    // We disable source address token validation in order to allow for 0-rtt
    // setup (plus source ip addresses are changing even during the connection
    // when ICE is used).
    quic_crypto_server_config_->set_validate_source_address_token(false);

    // Provide server with serialized config string to prove ownership.
    QuicCryptoServerConfig::ConfigOptions options;
    // The |message| is used to handle the return value of AddDefaultConfig
    // which is raw pointer of the CryptoHandshakeMessage.
    std::unique_ptr<CryptoHandshakeMessage> message(
        quic_crypto_server_config_->AddDefaultConfig(
            helper_->GetRandomGenerator(), helper_->GetClock(), options));
    quic_crypto_server_config_->set_pad_rej(false);
    quic_crypto_server_config_->set_pad_shlo(false);
  }
}

QuartcSession::~QuartcSession() {}

const QuicCryptoStream* QuartcSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

QuicCryptoStream* QuartcSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

QuartcStream* QuartcSession::CreateOutgoingBidirectionalStream() {
  // Use default priority for incoming QUIC streams.
  // TODO(zhihuang): Determine if this value is correct.
  return ActivateDataStream(CreateDataStream(
      GetNextOutgoingBidirectionalStreamId(), QuicStream::kDefaultPriority));
}

bool QuartcSession::SendOrQueueMessage(QuicString message) {
  if (!CanSendMessage()) {
    QUIC_LOG(ERROR) << "Quic session does not support SendMessage";
    return false;
  }

  if (message.size() > GetLargestMessagePayload()) {
    QUIC_LOG(ERROR) << "Message is too big, message_size=" << message.size()
                    << ", GetLargestMessagePayload="
                    << GetLargestMessagePayload();
    return false;
  }

  // There may be other messages in send queue, so we have to add message
  // to the queue and call queue processing helper.
  send_message_queue_.emplace_back(std::move(message));

  ProcessSendMessageQueue();

  return true;
}

void QuartcSession::ProcessSendMessageQueue() {
  while (!send_message_queue_.empty()) {
    MessageResult result = SendMessage(send_message_queue_.front());

    const size_t message_size = send_message_queue_.front().size();

    // Handle errors.
    switch (result.status) {
      case MESSAGE_STATUS_SUCCESS:
        QUIC_VLOG(1) << "Quartc message sent, message_id=" << result.message_id
                     << ", message_size=" << message_size;
        break;

      // If connection is congestion controlled or not writable yet, stop
      // send loop and we'll retry again when we get OnCanWrite notification.
      case MESSAGE_STATUS_ENCRYPTION_NOT_ESTABLISHED:
      case MESSAGE_STATUS_BLOCKED:
        QUIC_VLOG(1) << "Quartc message not sent because connection is blocked"
                     << ", message will be retried later, status="
                     << result.status << ", message_size=" << message_size;

        return;

      // Other errors are unexpected. We do not propagate error to Quartc,
      // because writes can be delayed.
      case MESSAGE_STATUS_UNSUPPORTED:
      case MESSAGE_STATUS_TOO_LARGE:
      case MESSAGE_STATUS_INTERNAL_ERROR:
        QUIC_DLOG(DFATAL)
            << "Failed to send quartc message due to unexpected error"
            << ", message will not be retried, status=" << result.status
            << ", message_size=" << message_size;
        break;
    }

    send_message_queue_.pop_front();
  }
}

void QuartcSession::OnCanWrite() {
  // TODO(b/119640244): Since we currently use messages for audio and streams
  // for video, it makes sense to process queued messages first, then call quic
  // core OnCanWrite, which will resend queued streams. Long term we may need
  // better solution especially if quic connection is used for both data and
  // media.

  // Process quartc messages that were previously blocked.
  ProcessSendMessageQueue();

  QuicSession::OnCanWrite();
}

void QuartcSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
  switch (event) {
    case ENCRYPTION_FIRST_ESTABLISHED:
    case ENCRYPTION_REESTABLISHED:
      // 1-rtt setup triggers 'ENCRYPTION_REESTABLISHED' (after REJ, when the
      // CHLO is sent).
      DCHECK(IsEncryptionEstablished());
      DCHECK(session_delegate_);
      session_delegate_->OnConnectionWritable();
      break;
    case HANDSHAKE_CONFIRMED:
      // On the server, handshake confirmed is the first time when you can start
      // writing packets.
      DCHECK(IsEncryptionEstablished());
      DCHECK(IsCryptoHandshakeConfirmed());

      DCHECK(session_delegate_);
      session_delegate_->OnConnectionWritable();
      session_delegate_->OnCryptoHandshakeComplete();
      break;
  }
}

void QuartcSession::CancelStream(QuicStreamId stream_id) {
  ResetStream(stream_id, QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
}

void QuartcSession::ResetStream(QuicStreamId stream_id,
                                QuicRstStreamErrorCode error) {
  if (!IsOpenStream(stream_id)) {
    return;
  }
  QuicStream* stream = QuicSession::GetOrCreateStream(stream_id);
  if (stream) {
    stream->Reset(error);
  }
}

void QuartcSession::OnCongestionWindowChange(QuicTime now) {
  DCHECK(session_delegate_);
  const RttStats* rtt_stats = connection_->sent_packet_manager().GetRttStats();

  QuicBandwidth bandwidth_estimate =
      connection_->sent_packet_manager().BandwidthEstimate();

  QuicByteCount in_flight =
      connection_->sent_packet_manager().GetBytesInFlight();
  QuicBandwidth pacing_rate =
      connection_->sent_packet_manager().GetSendAlgorithm()->PacingRate(
          in_flight);

  session_delegate_->OnCongestionControlChange(bandwidth_estimate, pacing_rate,
                                               rtt_stats->latest_rtt());
}

void QuartcSession::OnConnectionClosed(QuicErrorCode error,
                                       const QuicString& error_details,
                                       ConnectionCloseSource source) {
  QuicSession::OnConnectionClosed(error, error_details, source);
  DCHECK(session_delegate_);
  session_delegate_->OnConnectionClosed(error, error_details, source);

  // The session may be deleted after OnConnectionClosed(), so |this| must be
  // removed from the packet transport's delegate before it is deleted.
  packet_writer_->SetPacketTransportDelegate(nullptr);
}

void QuartcSession::SetPreSharedKey(QuicStringPiece key) {
  if (perspective_ == Perspective::IS_CLIENT) {
    quic_crypto_client_config_->set_pre_shared_key(key);
  } else {
    quic_crypto_server_config_->set_pre_shared_key(key);
  }
}

void QuartcSession::StartCryptoHandshake() {
  if (perspective_ == Perspective::IS_CLIENT) {
    QuicServerId server_id(unique_remote_server_id_, kQuicServerPort,
                           /*privacy_mode_enabled=*/false);
    QuicCryptoClientStream* crypto_stream = new QuicCryptoClientStream(
        server_id, this,
        quic_crypto_client_config_->proof_verifier()->CreateDefaultContext(),
        quic_crypto_client_config_.get(), this);
    crypto_stream_.reset(crypto_stream);
    QuicSession::Initialize();
    crypto_stream->CryptoConnect();
  } else {
    quic_compressed_certs_cache_.reset(new QuicCompressedCertsCache(
        QuicCompressedCertsCache::kQuicCompressedCertsCacheSize));
    bool use_stateless_rejects_if_peer_supported = false;
    QuicCryptoServerStream* crypto_stream = new QuicCryptoServerStream(
        quic_crypto_server_config_.get(), quic_compressed_certs_cache_.get(),
        use_stateless_rejects_if_peer_supported, this, &stream_helper_);
    crypto_stream_.reset(crypto_stream);
    QuicSession::Initialize();
  }

  // QUIC is ready to process incoming packets after QuicSession::Initialize().
  // Set the packet transport delegate to begin receiving packets.
  packet_writer_->SetPacketTransportDelegate(this);
}

void QuartcSession::CloseConnection(const QuicString& details) {
  connection_->CloseConnection(
      QuicErrorCode::QUIC_CONNECTION_CANCELLED, details,
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET_WITH_NO_ACK);
}

void QuartcSession::SetDelegate(Delegate* session_delegate) {
  if (session_delegate_) {
    LOG(WARNING) << "The delegate for the session has already been set.";
  }
  session_delegate_ = session_delegate;
  DCHECK(session_delegate_);
}

void QuartcSession::OnTransportCanWrite() {
  connection()->writer()->SetWritable();
  if (HasDataToWrite()) {
    connection()->OnCanWrite();
  }
}

void QuartcSession::OnTransportReceived(const char* data, size_t data_len) {
  QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(connection()->self_address(), connection()->peer_address(),
                   packet);
}

void QuartcSession::OnMessageReceived(QuicStringPiece message) {
  session_delegate_->OnMessageReceived(message);
}

void QuartcSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& cached) {
  // TODO(zhihuang): Handle the proof verification.
}

void QuartcSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& verify_details) {
  // TODO(zhihuang): Handle the proof verification.
}

QuicStream* QuartcSession::CreateIncomingStream(QuicStreamId id) {
  return ActivateDataStream(CreateDataStream(id, QuicStream::kDefaultPriority));
}

QuicStream* QuartcSession::CreateIncomingStream(PendingStream pending) {
  return ActivateDataStream(
      CreateDataStream(std::move(pending), QuicStream::kDefaultPriority));
}

std::unique_ptr<QuartcStream> QuartcSession::CreateDataStream(
    QuicStreamId id,
    spdy::SpdyPriority priority) {
  if (crypto_stream_ == nullptr || !crypto_stream_->encryption_established()) {
    // Encryption not active so no stream created
    return nullptr;
  }
  return InitializeDataStream(QuicMakeUnique<QuartcStream>(id, this), priority);
}

std::unique_ptr<QuartcStream> QuartcSession::CreateDataStream(
    PendingStream pending,
    spdy::SpdyPriority priority) {
  return InitializeDataStream(QuicMakeUnique<QuartcStream>(std::move(pending)),
                              priority);
}

std::unique_ptr<QuartcStream> QuartcSession::InitializeDataStream(
    std::unique_ptr<QuartcStream> stream,
    spdy::SpdyPriority priority) {
  // Register the stream to the QuicWriteBlockedList. |priority| is clamped
  // between 0 and 7, with 0 being the highest priority and 7 the lowest
  // priority.
  write_blocked_streams()->UpdateStreamPriority(stream->id(), priority);

  if (IsIncomingStream(stream->id())) {
    DCHECK(session_delegate_);
    // Incoming streams need to be registered with the session_delegate_.
    session_delegate_->OnIncomingStream(stream.get());
  }
  return stream;
}

QuartcStream* QuartcSession::ActivateDataStream(
    std::unique_ptr<QuartcStream> stream) {
  // Transfer ownership of the data stream to the session via ActivateStream().
  QuartcStream* raw = stream.release();
  if (raw) {
    // Make QuicSession take ownership of the stream.
    ActivateStream(std::unique_ptr<QuicStream>(raw));
  }
  return raw;
}

}  // namespace quic
