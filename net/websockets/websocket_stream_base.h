// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_

// This file is included from net/http files.
// Since net/http can be built without linking net/websockets code,
// this file should not depend on net/websockets.

#include <base/basictypes.h>

namespace net {

class ClientSocketHandle;
class SpdySession;
class WebSocketStream;

// WebSocketStreamBase is the base class of WebSocketStream.
// net/http code uses this interface to handle WebSocketStream.
class NET_EXPORT WebSocketStreamBase {
 public:
  class Factory {
   public:
    virtual ~Factory() {}

    // Create a WebSocketBasicStream.
    // This function (or the returned object) takes the ownership
    // of |connection|.
    virtual WebSocketStreamBase* CreateBasicStream(
        ClientSocketHandle* connection,
        bool using_proxy) = 0;

    // Create a WebSocketSpdyStream.
    virtual WebSocketStreamBase* CreateSpdyStream(
        const base::WeakPtr<SpdySession>& session,
        bool use_relative_url) = 0;
  };

  virtual ~WebSocketStreamBase() {}

  // Return this object as a WebSocketStream.
  virtual WebSocketStream* AsWebSocketStream() = 0;

 protected:
  WebSocketStreamBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamBase);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_BASE_H_
