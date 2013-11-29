// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
#define NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/websockets/websocket_stream.h"

class GURL;

namespace net {

class BoundNetLog;
class URLRequestContext;
class WebSocketHandshakeStreamCreateHelper;

class LinearCongruentialGenerator {
 public:
  explicit LinearCongruentialGenerator(uint32 seed);
  uint32 Generate();

 private:
  uint64 current_;
};

// Alternate version of WebSocketStream::CreateAndConnectStream() for testing
// use only. The difference is the use of a |create_helper| argument in place of
// |requested_subprotocols|. Implemented in websocket_stream.cc.
NET_EXPORT_PRIVATE extern scoped_ptr<WebSocketStreamRequest>
    CreateAndConnectStreamForTesting(
        const GURL& socket_url,
        scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper,
        const GURL& origin,
        URLRequestContext* url_request_context,
        const BoundNetLog& net_log,
        scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
