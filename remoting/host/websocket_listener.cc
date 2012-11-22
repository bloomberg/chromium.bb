// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/websocket_listener.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "remoting/host/websocket_connection.h"

namespace remoting {

namespace {
const int kTcpListenBacklog = 5;
}  // namespace

WebSocketListener::WebSocketListener()
    : tcp_socket_(new net::TCPServerSocket(NULL, net::NetLog::Source())),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

WebSocketListener::~WebSocketListener() {
  STLDeleteElements(&pending_connections_);
}

int WebSocketListener::Listen(const net::IPEndPoint& address,
                              const NewConnectionCallback& callback) {
  new_connection_callback_ = callback;

  int result = tcp_socket_->Listen(address, kTcpListenBacklog);
  if (result == net::OK) {
    DoAccept();
  }

  return result;
}

void WebSocketListener::DoAccept() {
  while (true) {
    int result = tcp_socket_->Accept(
        &accepted_socket_, base::Bind(&WebSocketListener::OnAccepted,
                                      base::Unretained(this)));
    if (result != net::ERR_IO_PENDING) {
      HandleAcceptResult(result);
    }
    if (result != net::OK) {
      break;
    }
  }
}

void WebSocketListener::OnAccepted(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (result == net::OK) {
    // The call for WebSocketConnection::Start() may result in delegate being
    // called which is allowed to destroyed this object, so we need to post a
    // task to call DoAccept() again asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&WebSocketListener::DoAccept,
                            weak_factory_.GetWeakPtr()));
  }
  HandleAcceptResult(result);
}

void WebSocketListener::HandleAcceptResult(int result) {
  if (result == net::OK) {
    WebSocketConnection* connection = new WebSocketConnection();
    pending_connections_.insert(connection);
    connection->Start(
        accepted_socket_.Pass(),
        base::Bind(&WebSocketListener::OnConnected, weak_factory_.GetWeakPtr(),
                   connection));
  } else {
    LOG(ERROR) << "Error when trying to accept WebSocket connection: "
               << result;
  }
}

void WebSocketListener::OnConnected(WebSocketConnection* connection_ptr,
                                    bool handshake_result) {
  scoped_ptr<WebSocketConnection> connection(connection_ptr);
  pending_connections_.erase(connection_ptr);
  if (handshake_result) {
    new_connection_callback_.Run(connection.Pass());
  }
}

}  // namespace remoting
