// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/tools/quic_simple_client_session.h"

#include <utility>

namespace quic {

QuicSimpleClientSession::QuicSimpleClientSession(
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicConnection* connection,
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config,
    QuicClientPushPromiseIndex* push_promise_index,
    bool drop_response_body)
    : QuicSimpleClientSession(config,
                              supported_versions,
                              connection,
                              server_id,
                              crypto_config,
                              push_promise_index,
                              drop_response_body,
                              /*enable_web_transport=*/false) {}

QuicSimpleClientSession::QuicSimpleClientSession(
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicConnection* connection,
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config,
    QuicClientPushPromiseIndex* push_promise_index,
    bool drop_response_body,
    bool enable_web_transport)
    : QuicSpdyClientSession(config,
                            supported_versions,
                            connection,
                            server_id,
                            crypto_config,
                            push_promise_index),
      drop_response_body_(drop_response_body),
      enable_web_transport_(enable_web_transport) {}

std::unique_ptr<QuicSpdyClientStream>
QuicSimpleClientSession::CreateClientStream() {
  return std::make_unique<QuicSimpleClientStream>(
      GetNextOutgoingBidirectionalStreamId(), this, BIDIRECTIONAL,
      drop_response_body_);
}

bool QuicSimpleClientSession::ShouldNegotiateWebTransport() {
  return enable_web_transport_;
}

bool QuicSimpleClientSession::ShouldNegotiateHttp3Datagram() {
  return enable_web_transport_;
}

}  // namespace quic
