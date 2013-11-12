// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_

// This file is included from net/http files.
// Since net/http can be built without linking net/websockets code,
// this file must not introduce any link-time dependencies on websockets.

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_stream_base.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class ClientSocketHandle;
class SpdySession;

// WebSocketHandshakeStreamBase is the base class of
// WebSocketBasicHandshakeStream.  net/http code uses this interface to handle
// WebSocketBasicHandshakeStream when it needs to be treated differently from
// HttpStreamBase.
class NET_EXPORT WebSocketHandshakeStreamBase : public HttpStreamBase {
 public:
  class CreateHelper {
   public:
    virtual ~CreateHelper() {}

    // Create a WebSocketBasicHandshakeStream. This function (or the returned
    // object) takes the ownership of |connection|. This is called after the
    // underlying connection has been established but before any handshake data
    // has been transferred.
    virtual WebSocketHandshakeStreamBase* CreateBasicStream(
        ClientSocketHandle* connection,
        bool using_proxy) = 0;

    // Create a WebSocketSpdyHandshakeStream (unimplemented as of October 2013)
    virtual WebSocketHandshakeStreamBase* CreateSpdyStream(
        const base::WeakPtr<SpdySession>& session,
        bool use_relative_url) = 0;
  };

  virtual ~WebSocketHandshakeStreamBase() {}

  // After the handshake has completed, this method creates a WebSocketStream
  // (of the appropriate type) from the WebSocketHandshakeStreamBase object.
  // The WebSocketHandshakeStreamBase object is unusable after Upgrade() has
  // been called.
  virtual scoped_ptr<WebSocketStream> Upgrade() = 0;

 protected:
  WebSocketHandshakeStreamBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeStreamBase);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_
