// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include "base/logging.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

using std::string;

namespace net {
namespace tools {

QuicClientSession::QuicClientSession(const QuicConfig& config,
                                     QuicConnection* connection)
    : QuicClientSessionBase(connection, config), respect_goaway_(true) {
}

QuicClientSession::~QuicClientSession() {
}

void QuicClientSession::InitializeSession(
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config) {
  crypto_stream_.reset(
      new QuicCryptoClientStream(server_id, this, nullptr, crypto_config));
  QuicClientSessionBase::InitializeSession();
}

void QuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& /*cached*/) {}

void QuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& /*verify_details*/) {}

QuicSpdyClientStream* QuicClientSession::CreateOutgoingDataStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return nullptr;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenStreams() << " open.";
    return nullptr;
  }
  if (goaway_received() && respect_goaway_) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return nullptr;
  }
  QuicSpdyClientStream* stream
      = new QuicSpdyClientStream(GetNextStreamId(), this);
  ActivateStream(stream);
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
}

void QuicClientSession::CryptoConnect() {
  DCHECK(flow_controller());
  crypto_stream_->CryptoConnect();
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

QuicDataStream* QuicClientSession::CreateIncomingDataStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return nullptr;
}

}  // namespace tools
}  // namespace net
