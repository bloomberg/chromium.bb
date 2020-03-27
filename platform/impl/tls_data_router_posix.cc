// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_posix.h"
#include "util/logging.h"

namespace openscreen {

TlsDataRouterPosix::TlsDataRouterPosix(
    SocketHandleWaiter* waiter,
    std::function<Clock::time_point()> now_function)
    : waiter_(waiter), now_function_(now_function) {}

TlsDataRouterPosix::~TlsDataRouterPosix() {
  waiter_->UnsubscribeAll(this);
}

void TlsDataRouterPosix::RegisterConnection(TlsConnectionPosix* connection) {
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    OSP_DCHECK(std::find(connections_.begin(), connections_.end(),
                         connection) == connections_.end());
    connections_.push_back(connection);
  }

  waiter_->Subscribe(this, connection->socket_handle());
}

void TlsDataRouterPosix::DeregisterConnection(TlsConnectionPosix* connection) {
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = std::remove_if(
        connections_.begin(), connections_.end(),
        [connection](TlsConnectionPosix* conn) { return conn == connection; });
    if (it == connections_.end()) {
      return;
    }
    connections_.erase(it, connections_.end());
  }

  waiter_->OnHandleDeletion(this, connection->socket_handle());
}

void TlsDataRouterPosix::RegisterAcceptObserver(
    std::unique_ptr<StreamSocketPosix> socket,
    SocketObserver* observer) {
  OSP_DCHECK(observer);
  StreamSocketPosix* socket_ptr = socket.get();
  {
    std::unique_lock<std::mutex> lock(accept_socket_mutex_);
    accept_stream_sockets_.push_back(std::move(socket));
    accept_socket_mappings_[socket_ptr] = observer;
  }

  waiter_->Subscribe(this, socket_ptr->socket_handle());
}

void TlsDataRouterPosix::DeregisterAcceptObserver(StreamSocketPosix* socket) {
  OnSocketDestroyed(socket, false);
}

void TlsDataRouterPosix::OnConnectionDestroyed(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket,
                                           bool skip_locking_for_testing) {
  {
    std::unique_lock<std::mutex> lock(accept_socket_mutex_);
    if (!accept_socket_mappings_.erase(socket)) {
      return;
    }
  }

  waiter_->OnHandleDeletion(this, std::cref(socket->socket_handle()),
                            skip_locking_for_testing);

  {
    std::unique_lock<std::mutex> lock(accept_socket_mutex_);
    auto it = std::find_if(
        accept_stream_sockets_.begin(), accept_stream_sockets_.end(),
        [socket](const std::unique_ptr<StreamSocketPosix>& ptr) {
          return ptr.get() == socket;
        });
    OSP_DCHECK(it != accept_stream_sockets_.end());
    accept_stream_sockets_.erase(it);
  }
}

void TlsDataRouterPosix::ProcessReadyHandle(
    SocketHandleWaiter::SocketHandleRef handle) {
  {
    std::unique_lock<std::mutex> lock(accept_socket_mutex_);
    for (const auto& pair : accept_socket_mappings_) {
      if (pair.first->socket_handle() == handle) {
        pair.second->OnConnectionPending(pair.first);
        return;
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (TlsConnectionPosix* connection : connections_) {
      if (connection->socket_handle() == handle) {
        connection->TryReceiveMessage();
        connection->SendAvailableBytes();
        return;
      }
    }
  }
}

bool TlsDataRouterPosix::HasTimedOut(Clock::time_point start_time,
                                     Clock::duration timeout) {
  return now_function_() - start_time > timeout;
}

void TlsDataRouterPosix::RemoveWatchedSocket(StreamSocketPosix* socket) {
  std::unique_lock<std::mutex> lock(accept_socket_mutex_);
  const auto it = accept_socket_mappings_.find(socket);
  if (it != accept_socket_mappings_.end()) {
    accept_socket_mappings_.erase(it);
  }
}

bool TlsDataRouterPosix::IsSocketWatched(StreamSocketPosix* socket) const {
  std::unique_lock<std::mutex> lock(accept_socket_mutex_);
  return accept_socket_mappings_.find(socket) != accept_socket_mappings_.end();
}

}  // namespace openscreen
