// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream_adapters.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "net/base/io_buffer.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket.h"

namespace net {

WebSocketClientSocketHandleAdapter::WebSocketClientSocketHandleAdapter(
    std::unique_ptr<ClientSocketHandle> connection)
    : connection_(std::move(connection)) {}

WebSocketClientSocketHandleAdapter::~WebSocketClientSocketHandleAdapter() {}

int WebSocketClientSocketHandleAdapter::Read(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  return connection_->socket()->Read(buf, buf_len, callback);
}

int WebSocketClientSocketHandleAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return connection_->socket()->Write(buf, buf_len, callback,
                                      traffic_annotation);
}

void WebSocketClientSocketHandleAdapter::Disconnect() {
  connection_->socket()->Disconnect();
}

bool WebSocketClientSocketHandleAdapter::is_initialized() const {
  return connection_->is_initialized();
}

}  // namespace net
