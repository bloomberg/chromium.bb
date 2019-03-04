// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_session.h"

#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_mem_slice_storage.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/quartc/quartc_crypto_helpers.h"

namespace quic {
namespace {

// Arbitrary server port number for net::QuicCryptoClientConfig.
const int kQuicServerPort = 0;

}  // namespace

QuartcSession::QuartcSession(std::unique_ptr<QuicConnection> connection,
                             Visitor* visitor,
                             const QuicConfig& config,
                             const ParsedQuicVersionVector& supported_versions,
                             const QuicClock* clock)
    : QuicSession(connection.get(), visitor, config, supported_versions),
      connection_(std::move(connection)),
      clock_(clock),
      per_packet_options_(QuicMakeUnique<QuartcPerPacketOptions>()) {
  per_packet_options_->connection = connection_.get();
  connection_->set_per_packet_options(per_packet_options_.get());
}

QuartcSession::~QuartcSession() {}

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
    struct iovec iov = {const_cast<char*>(send_message_queue_.front().data()),
                        send_message_queue_.front().length()};
    QuicMemSliceStorage storage(
        &iov, 1, connection()->helper()->GetStreamSendBufferAllocator(),
        send_message_queue_.front().length());
    MessageResult result = SendMessage(storage.ToSpan());

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

bool QuartcSession::ShouldKeepConnectionAlive() const {
  // TODO(mellem): Quartc may want different keepalive logic than HTTP.
  return GetNumOpenDynamicStreams() > 0;
}

void QuartcSession::OnConnectionClosed(QuicErrorCode error,
                                       const QuicString& error_details,
                                       ConnectionCloseSource source) {
  QuicSession::OnConnectionClosed(error, error_details, source);
  DCHECK(session_delegate_);
  session_delegate_->OnConnectionClosed(error, error_details, source);
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
  if (GetCryptoStream() == nullptr ||
      !GetCryptoStream()->encryption_established()) {
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

QuartcClientSession::QuartcClientSession(
    std::unique_ptr<QuicConnection> connection,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    const QuicClock* clock,
    std::unique_ptr<QuartcPacketWriter> packet_writer,
    std::unique_ptr<QuicCryptoClientConfig> client_crypto_config,
    QuicStringPiece server_crypto_config)
    : QuartcSession(std::move(connection),
                    /*visitor=*/nullptr,
                    config,
                    supported_versions,
                    clock),
      packet_writer_(std::move(packet_writer)),
      client_crypto_config_(std::move(client_crypto_config)),
      server_config_(server_crypto_config) {
  DCHECK_EQ(QuartcSession::connection()->perspective(), Perspective::IS_CLIENT);
}

QuartcClientSession::~QuartcClientSession() {
  // The client session is the packet transport delegate, so it must be unset
  // before the session is deleted.
  packet_writer_->SetPacketTransportDelegate(nullptr);
}

void QuartcClientSession::Initialize() {
  DCHECK(crypto_stream_) << "Do not call QuartcSession::Initialize(), call "
                            "StartCryptoHandshake() instead.";
  QuartcSession::Initialize();

  // QUIC is ready to process incoming packets after Initialize().
  // Set the packet transport delegate to begin receiving packets.
  packet_writer_->SetPacketTransportDelegate(this);
}

const QuicCryptoStream* QuartcClientSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

QuicCryptoStream* QuartcClientSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

void QuartcClientSession::StartCryptoHandshake() {
  QuicServerId server_id(/*host=*/"", kQuicServerPort,
                         /*privacy_mode_enabled=*/false);

  if (!server_config_.empty()) {
    QuicCryptoServerConfig::ConfigOptions options;

    QuicString error;
    QuicWallTime now = clock()->WallNow();
    QuicCryptoClientConfig::CachedState::ServerConfigState result =
        client_crypto_config_->LookupOrCreate(server_id)->SetServerConfig(
            server_config_, now,
            /*expiry_time=*/now.Add(QuicTime::Delta::Infinite()), &error);

    if (result == QuicCryptoClientConfig::CachedState::SERVER_CONFIG_VALID) {
      DCHECK_EQ(error, "");
      client_crypto_config_->LookupOrCreate(server_id)->SetProof(
          std::vector<QuicString>{kDummyCertName}, /*cert_sct=*/"",
          /*chlo_hash=*/"", /*signature=*/"anything");
    } else {
      LOG(DFATAL) << "Unable to set server config, error=" << error;
    }
  }

  crypto_stream_ = QuicMakeUnique<QuicCryptoClientStream>(
      server_id, this,
      client_crypto_config_->proof_verifier()->CreateDefaultContext(),
      client_crypto_config_.get(), this);
  Initialize();
  crypto_stream_->CryptoConnect();
}

void QuartcClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& cached) {
  // TODO(zhihuang): Handle the proof verification.
}

void QuartcClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& verify_details) {
  // TODO(zhihuang): Handle the proof verification.
}

QuartcServerSession::QuartcServerSession(
    std::unique_ptr<QuicConnection> connection,
    Visitor* visitor,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    const QuicClock* clock,
    const QuicCryptoServerConfig* server_crypto_config,
    QuicCompressedCertsCache* const compressed_certs_cache,
    QuicCryptoServerStream::Helper* const stream_helper)
    : QuartcSession(std::move(connection),
                    visitor,
                    config,
                    supported_versions,
                    clock),
      server_crypto_config_(server_crypto_config),
      compressed_certs_cache_(compressed_certs_cache),
      stream_helper_(stream_helper) {
  DCHECK_EQ(QuartcSession::connection()->perspective(), Perspective::IS_SERVER);
}

const QuicCryptoStream* QuartcServerSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

QuicCryptoStream* QuartcServerSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

void QuartcServerSession::StartCryptoHandshake() {
  crypto_stream_ = QuicMakeUnique<QuicCryptoServerStream>(
      server_crypto_config_, compressed_certs_cache_,
      /*use_stateless_rejects_if_peer_supported=*/false, this, stream_helper_);
  Initialize();
}

}  // namespace quic
