// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_create_helper.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/socket/client_socket_handle.h"
#include "net/spdy/spdy_session.h"
#include "net/websockets/websocket_basic_handshake_stream.h"

namespace net {

WebSocketHandshakeStreamCreateHelper::WebSocketHandshakeStreamCreateHelper(
    const std::vector<std::string>& requested_subprotocols)
    : requested_subprotocols_(requested_subprotocols),
      stream_(NULL) {}

WebSocketHandshakeStreamCreateHelper::~WebSocketHandshakeStreamCreateHelper() {}

WebSocketHandshakeStreamBase*
WebSocketHandshakeStreamCreateHelper::CreateBasicStream(
    scoped_ptr<ClientSocketHandle> connection,
    bool using_proxy) {
  return stream_ =
      new WebSocketBasicHandshakeStream(connection.Pass(),
                                        using_proxy,
                                        requested_subprotocols_,
                                        std::vector<std::string>());
}

// TODO(ricea): Create a WebSocketSpdyHandshakeStream. crbug.com/323852
WebSocketHandshakeStreamBase*
WebSocketHandshakeStreamCreateHelper::CreateSpdyStream(
    const base::WeakPtr<SpdySession>& session,
    bool use_relative_url) {
  NOTREACHED() << "Not implemented";
  return NULL;
}

}  // namespace net
