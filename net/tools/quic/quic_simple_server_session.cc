// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_session.h"

#include "base/logging.h"
#include "net/quic/proto/cached_network_parameters.pb.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_spdy_session.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/tools/quic/quic_simple_server_stream.h"

namespace net {
namespace tools {

QuicSimpleServerSession::QuicSimpleServerSession(
    const QuicConfig& config,
    QuicConnection* connection,
    QuicServerSessionVisitor* visitor,
    const QuicCryptoServerConfig* crypto_config)
    : QuicServerSessionBase(config, connection, visitor, crypto_config) {}

QuicSimpleServerSession::~QuicSimpleServerSession() {}

QuicCryptoServerStreamBase*
QuicSimpleServerSession::CreateQuicCryptoServerStream(
    const QuicCryptoServerConfig* crypto_config) {
  return new QuicCryptoServerStream(crypto_config, this);
}

QuicSpdyStream* QuicSimpleServerSession::CreateIncomingDynamicStream(
    QuicStreamId id) {
  if (!ShouldCreateIncomingDynamicStream(id)) {
    return nullptr;
  }

  return new QuicSimpleServerStream(id, this);
}

QuicSimpleServerStream* QuicSimpleServerSession::CreateOutgoingDynamicStream(
    SpdyPriority priority) {
  if (!ShouldCreateOutgoingDynamicStream()) {
    return nullptr;
  }

  QuicSimpleServerStream* stream =
      new QuicSimpleServerStream(GetNextOutgoingStreamId(), this);
  stream->SetPriority(priority);
  ActivateStream(stream);
  return stream;
}

}  // namespace tools
}  // namespace net
