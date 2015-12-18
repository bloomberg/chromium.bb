// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include "base/logging.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

using std::string;

namespace net {
namespace tools {

QuicClientSession::QuicClientSession(const QuicConfig& config,
                                     QuicConnection* connection,
                                     const QuicServerId& server_id,
                                     QuicCryptoClientConfig* crypto_config)
    : QuicClientSessionBase(connection, config),
      server_id_(server_id),
      crypto_config_(crypto_config),
      respect_goaway_(true) {}

QuicClientSession::~QuicClientSession() {}

void QuicClientSession::Initialize() {
  crypto_stream_.reset(CreateQuicCryptoStream());
  QuicClientSessionBase::Initialize();
}

void QuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& /*cached*/) {}

void QuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& /*verify_details*/) {}

QuicSpdyClientStream* QuicClientSession::CreateOutgoingDynamicStream(
    SpdyPriority priority) {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return nullptr;
  }
  if (GetNumOpenOutgoingStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenOutgoingStreams() << " open.";
    return nullptr;
  }
  if (goaway_received() && respect_goaway_) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return nullptr;
  }
  QuicSpdyClientStream* stream = CreateClientStream();
  stream->SetPriority(priority);
  ActivateStream(stream);
  return stream;
}

QuicSpdyClientStream* QuicClientSession::CreateClientStream() {
  return new QuicSpdyClientStream(GetNextOutgoingStreamId(), this);
}

QuicCryptoClientStreamBase* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
}

void QuicClientSession::CryptoConnect() {
  DCHECK(flow_controller());
  crypto_stream_->CryptoConnect();
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

QuicSpdyStream* QuicClientSession::CreateIncomingDynamicStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return nullptr;
}

QuicCryptoClientStreamBase* QuicClientSession::CreateQuicCryptoStream() {
  return new QuicCryptoClientStream(
      server_id_, this, new ProofVerifyContextChromium(0, BoundNetLog()),
      crypto_config_);
}

}  // namespace tools

}  // namespace net
