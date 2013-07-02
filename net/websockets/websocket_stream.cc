// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include "base/logging.h"

namespace net {

WebSocketStreamRequest::~WebSocketStreamRequest() {}

WebSocketStream::WebSocketStream() {}
WebSocketStream::~WebSocketStream() {}

WebSocketStream::ConnectDelegate::~ConnectDelegate() {}

// Placeholder until the real implementation is ready.
scoped_ptr<WebSocketStreamRequest> WebSocketStream::CreateAndConnectStream(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<ConnectDelegate> connect_delegate) {
  NOTIMPLEMENTED();
  return make_scoped_ptr(new WebSocketStreamRequest());
}

WebSocketStream* WebSocketStream::AsWebSocketStream() { return this; }

}  // namespace net
