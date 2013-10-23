// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_

// This file is included from net/http files.
// Since net/http can be built without linking net/websockets code,
// this file should not depend on net/websockets.

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_stream_base.h"

namespace net {

class ClientSocketHandle;
class SpdySession;

// WebSocketStreamBase is the base class of WebSocketBasicHandshakeStream.
// net/http code uses this interface to handle WebSocketBasicHandshakeStream
// when it needs to be treated differently from HttpStreamBase.
class NET_EXPORT WebSocketStreamBase : public HttpStreamBase {
 public:
  class Factory {
   public:
    virtual ~Factory() {}

    // Create a WebSocketBasicHandshakeStream. This function (or the returned
    // object) takes the ownership of |connection|. This is called after the
    // underlying connection has been established but before any handshake data
    // has been transferred.
    virtual WebSocketStreamBase* CreateBasicStream(
        ClientSocketHandle* connection,
        bool using_proxy) = 0;

    // Create a WebSocketSpdyHandshakeStream (unimplemented as of October 2013)
    virtual WebSocketStreamBase* CreateSpdyStream(
        const base::WeakPtr<SpdySession>& session,
        bool use_relative_url) = 0;
  };

  virtual ~WebSocketStreamBase() {}

 protected:
  WebSocketStreamBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamBase);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_
