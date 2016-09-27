// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_base.h"

#include "base/strings/string_number_conversions.h"
#include "net/quic/core/crypto/quic_random.h"
#include "net/quic/core/quic_server_id.h"

using base::StringPiece;
using base::StringToInt;
using std::string;

namespace net {

void QuicClientBase::ClientQuicDataToResend::Resend() {
  client_->SendRequest(*headers_, body_, fin_);
  headers_ = nullptr;
}

QuicClientBase::QuicDataToResend::QuicDataToResend(
    std::unique_ptr<SpdyHeaderBlock> headers,
    StringPiece body,
    bool fin)
    : headers_(std::move(headers)), body_(body), fin_(fin) {}

QuicClientBase::QuicDataToResend::~QuicDataToResend() {}

QuicClientBase::QuicClientBase(const QuicServerId& server_id,
                               const QuicVersionVector& supported_versions,
                               const QuicConfig& config,
                               QuicConnectionHelperInterface* helper,
                               QuicAlarmFactory* alarm_factory,
                               std::unique_ptr<ProofVerifier> proof_verifier)
    : server_id_(server_id),
      local_port_(0),
      config_(config),
      crypto_config_(std::move(proof_verifier)),
      helper_(helper),
      alarm_factory_(alarm_factory),
      supported_versions_(supported_versions),
      initial_max_packet_length_(0),
      num_stateless_rejects_received_(0),
      num_sent_client_hellos_(0),
      connection_error_(QUIC_NO_ERROR),
      connected_or_attempting_connect_(false),
      store_response_(false),
      latest_response_code_(-1) {}

QuicClientBase::~QuicClientBase() {}

void QuicClientBase::OnClose(QuicSpdyStream* stream) {
  DCHECK(stream != nullptr);
  QuicSpdyClientStream* client_stream =
      static_cast<QuicSpdyClientStream*>(stream);

  const SpdyHeaderBlock& response_headers = client_stream->response_headers();
  if (response_listener_ != nullptr) {
    response_listener_->OnCompleteResponse(stream->id(), response_headers,
                                           client_stream->data());
  }

  // Store response headers and body.
  if (store_response_) {
    auto status = response_headers.find(":status");
    if (status == response_headers.end() ||
        !StringToInt(status->second, &latest_response_code_)) {
      LOG(ERROR) << "Invalid response headers";
    }
    latest_response_headers_ = response_headers.DebugString();
    latest_response_body_ = client_stream->data();
    latest_response_trailers_ =
        client_stream->received_trailers().DebugString();
  }
}

bool QuicClientBase::Initialize() {
  num_sent_client_hellos_ = 0;
  num_stateless_rejects_received_ = 0;
  connection_error_ = QUIC_NO_ERROR;
  connected_or_attempting_connect_ = false;
  return true;
}

ProofVerifier* QuicClientBase::proof_verifier() const {
  return crypto_config_.proof_verifier();
}

QuicClientSession* QuicClientBase::CreateQuicClientSession(
    QuicConnection* connection) {
  session_.reset(new QuicClientSession(config_, connection, server_id_,
                                       &crypto_config_, &push_promise_index_));
  if (initial_max_packet_length_ != 0) {
    session()->connection()->SetMaxPacketLength(initial_max_packet_length_);
  }
  return session_.get();
}

bool QuicClientBase::EncryptionBeingEstablished() {
  return !session_->IsEncryptionEstablished() &&
         session_->connection()->connected();
}

QuicSpdyClientStream* QuicClientBase::CreateReliableClientStream() {
  if (!connected()) {
    return nullptr;
  }

  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDynamicStream(kDefaultPriority);
  if (stream) {
    stream->set_visitor(this);
  }
  return stream;
}

void QuicClientBase::WaitForStreamToClose(QuicStreamId id) {
  DCHECK(connected());

  while (connected() && !session_->IsClosedStream(id)) {
    WaitForEvents();
  }
}

void QuicClientBase::WaitForCryptoHandshakeConfirmed() {
  DCHECK(connected());

  while (connected() && !session_->IsCryptoHandshakeConfirmed()) {
    WaitForEvents();
  }
}

bool QuicClientBase::connected() const {
  return session_.get() && session_->connection() &&
         session_->connection()->connected();
}

bool QuicClientBase::goaway_received() const {
  return session_ != nullptr && session_->goaway_received();
}

int QuicClientBase::GetNumSentClientHellos() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  const int current_session_hellos = !connected_or_attempting_connect_
                                         ? 0
                                         : session_->GetNumSentClientHellos();
  return num_sent_client_hellos_ + current_session_hellos;
}

void QuicClientBase::UpdateStats() {
  num_sent_client_hellos_ += session()->GetNumSentClientHellos();
  if (session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    ++num_stateless_rejects_received_;
  }
}

int QuicClientBase::GetNumReceivedServerConfigUpdates() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  // We do not need to take stateless rejects into account, since we
  // don't expect any scup messages to be sent during a
  // statelessly-rejected connection.
  return !connected_or_attempting_connect_
             ? 0
             : session_->GetNumReceivedServerConfigUpdates();
}

QuicErrorCode QuicClientBase::connection_error() const {
  // Return the high-level error if there was one.  Otherwise, return the
  // connection error from the last session.
  if (connection_error_ != QUIC_NO_ERROR) {
    return connection_error_;
  }
  if (session_.get() == nullptr) {
    return QUIC_NO_ERROR;
  }
  return session_->error();
}

QuicConnectionId QuicClientBase::GetNextConnectionId() {
  QuicConnectionId server_designated_id = GetNextServerDesignatedConnectionId();
  return server_designated_id ? server_designated_id
                              : GenerateNewConnectionId();
}

QuicConnectionId QuicClientBase::GetNextServerDesignatedConnectionId() {
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id_);
  // If the cached state indicates that we should use a server-designated
  // connection ID, then return that connection ID.
  CHECK(cached != nullptr) << "QuicClientCryptoConfig::LookupOrCreate returned "
                           << "unexpected nullptr.";
  return cached->has_server_designated_connection_id()
             ? cached->GetNextServerDesignatedConnectionId()
             : 0;
}

QuicConnectionId QuicClientBase::GenerateNewConnectionId() {
  return QuicRandom::GetInstance()->RandUint64();
}

void QuicClientBase::MaybeAddDataToResend(const SpdyHeaderBlock& headers,
                                          StringPiece body,
                                          bool fin) {
  if (!FLAGS_enable_quic_stateless_reject_support) {
    return;
  }

  if (session()->IsCryptoHandshakeConfirmed()) {
    // The handshake is confirmed.  No need to continue saving requests to
    // resend.
    data_to_resend_on_connect_.clear();
    return;
  }

  // The handshake is not confirmed.  Push the data onto the queue of data to
  // resend if statelessly rejected.
  std::unique_ptr<SpdyHeaderBlock> new_headers(
      new SpdyHeaderBlock(headers.Clone()));
  std::unique_ptr<QuicDataToResend> data_to_resend(
      new ClientQuicDataToResend(std::move(new_headers), body, fin, this));
  MaybeAddQuicDataToResend(std::move(data_to_resend));
}

void QuicClientBase::MaybeAddQuicDataToResend(
    std::unique_ptr<QuicDataToResend> data_to_resend) {
  data_to_resend_on_connect_.push_back(std::move(data_to_resend));
}

void QuicClientBase::ClearDataToResend() {
  data_to_resend_on_connect_.clear();
}

void QuicClientBase::ResendSavedData() {
  // Calling Resend will re-enqueue the data, so swap out
  //  data_to_resend_on_connect_ before iterating.
  std::vector<std::unique_ptr<QuicDataToResend>> old_data;
  old_data.swap(data_to_resend_on_connect_);
  for (const auto& data : old_data) {
    data->Resend();
  }
  data_to_resend_on_connect_.clear();
}

void QuicClientBase::AddPromiseDataToResend(const SpdyHeaderBlock& headers,
                                            StringPiece body,
                                            bool fin) {
  std::unique_ptr<SpdyHeaderBlock> new_headers(
      new SpdyHeaderBlock(headers.Clone()));
  push_promise_data_to_resend_.reset(
      new ClientQuicDataToResend(std::move(new_headers), body, fin, this));
}

bool QuicClientBase::CheckVary(const SpdyHeaderBlock& client_request,
                               const SpdyHeaderBlock& promise_request,
                               const SpdyHeaderBlock& promise_response) {
  return true;
}

void QuicClientBase::OnRendezvousResult(QuicSpdyStream* stream) {
  std::unique_ptr<ClientQuicDataToResend> data_to_resend =
      std::move(push_promise_data_to_resend_);
  if (stream) {
    stream->set_visitor(this);
    stream->OnDataAvailable();
  } else if (data_to_resend.get()) {
    data_to_resend->Resend();
  }
}

size_t QuicClientBase::latest_response_code() const {
  QUIC_BUG_IF(!store_response_) << "Response not stored!";
  return latest_response_code_;
}

const string& QuicClientBase::latest_response_headers() const {
  QUIC_BUG_IF(!store_response_) << "Response not stored!";
  return latest_response_headers_;
}

const SpdyHeaderBlock& QuicClientBase::latest_response_header_block() const {
  QUIC_BUG_IF(!store_response_) << "Response not stored!";
  return latest_response_header_block_;
}

const string& QuicClientBase::latest_response_body() const {
  QUIC_BUG_IF(!store_response_) << "Response not stored!";
  return latest_response_body_;
}

const string& QuicClientBase::latest_response_trailers() const {
  QUIC_BUG_IF(!store_response_) << "Response not stored!";
  return latest_response_trailers_;
}

}  // namespace net
