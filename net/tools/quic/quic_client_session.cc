// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include "base/logging.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/tools/quic/quic_reliable_client_stream.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

using std::string;

namespace net {
namespace tools {

QuicClientSession::QuicClientSession(
    const string& server_hostname,
    const QuicConfig& config,
    QuicConnection* connection,
    QuicCryptoClientConfig* crypto_config)
    : QuicSession(connection, false),
      crypto_stream_(server_hostname, config, this, crypto_config) {
}

QuicClientSession::~QuicClientSession() {
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingReliableStream() {
  if (!crypto_stream_.encryption_established()) {
    DLOG(INFO) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already received goaway.";
    return NULL;
  }
  QuicReliableClientStream* stream
      = new QuicSpdyClientStream(GetNextStreamId(), this);
  ActivateStream(stream);
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return &crypto_stream_;
}

bool QuicClientSession::CryptoConnect() {
  return crypto_stream_.CryptoConnect();
}

ReliableQuicStream* QuicClientSession::CreateIncomingReliableStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

}  // namespace tools
}  // namespace net
