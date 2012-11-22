// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBSOCKET_LISTENER_H_
#define REMOTING_HOST_WEBSOCKET_LISTENER_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/socket/tcp_server_socket.h"

namespace net {
class IPEndPoint;
class StreamSocket;
}  // namespace net

namespace remoting {

class WebSocketConnection;

class WebSocketListener {
 public:
  typedef base::Callback<
      void(scoped_ptr<WebSocketConnection> connection)> NewConnectionCallback;

  WebSocketListener();
  ~WebSocketListener();

  // Starts listening on the specified address and returns net::Error code in
  // case of a failure. When successful, the specified |callback| will be called
  // for each incoming until the listener is destroyed.
  int Listen(const net::IPEndPoint& address,
             const NewConnectionCallback& callback);

 private:
  // Calls ServerSocket::Accept() to accept new connections.
  void DoAccept();

  // Callback for ServerSocket::Accept().
  void OnAccepted(int result);

  // Handles Accept() result from DoAccept() or OnAccepted().
  void HandleAcceptResult(int result);

  // Callback for WebSocketConnection::Start().
  void OnConnected(WebSocketConnection* connection_ptr, bool handshake_result);

  scoped_ptr<net::TCPServerSocket> tcp_socket_;
  NewConnectionCallback new_connection_callback_;

  // Object passed to Accept() when accepting incoming connections.
  scoped_ptr<net::StreamSocket> accepted_socket_;

  // Incoming connections that have been accepted, but for which we haven't
  // received headers yet and haven't called |new_connection_callback_|. They
  // are owned by the listener until we receive headers and call the callback.
  std::set<WebSocketConnection*> pending_connections_;

  base::WeakPtrFactory<WebSocketListener> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketListener);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBSOCKET_LISTENER_H_
