// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "platform/api/logging.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_posix.h"

namespace openscreen {
namespace platform {

TlsDataRouterPosix::TlsDataRouterPosix(NetworkWaiter* waiter)
    : waiter_(waiter) {}

TlsDataRouterPosix::~TlsDataRouterPosix() {
  waiter_->UnsubscribeAll(this);
}

void TlsDataRouterPosix::RegisterConnection(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::DeregisterConnection(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::RegisterSocketObserver(StreamSocketPosix* socket,
                                                SocketObserver* observer) {
  OSP_DCHECK(socket);
  OSP_DCHECK(observer);
  {
    std::unique_lock<std::mutex> lock(socket_mutex_);
    socket_mappings_[socket] = observer;
  }

  waiter_->Subscribe(this, socket->socket_handle());
}

void TlsDataRouterPosix::DeregisterSocketObserver(StreamSocketPosix* socket) {
  RemoveWatchedSocket(socket);
  waiter_->Unsubscribe(this, socket->socket_handle());
}

void TlsDataRouterPosix::OnConnectionDestroyed(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket) {
  OnSocketDestroyed(socket, false);
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket,
                                           bool skip_locking_for_testing) {
  RemoveWatchedSocket(socket);
  waiter_->OnHandleDeletion(this, std::cref(socket->socket_handle()),
                            skip_locking_for_testing);
}

void TlsDataRouterPosix::ReadAll() {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  for (TlsConnectionPosix* connection : connections_) {
    connection->TryReceiveMessage();
  }
}

void TlsDataRouterPosix::WriteAll() {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  for (TlsConnectionPosix* connection : connections_) {
    connection->SendAvailableBytes();
  }
}

void TlsDataRouterPosix::ProcessReadyHandle(
    NetworkWaiter::SocketHandleRef handle) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  for (const auto& pair : socket_mappings_) {
    if (pair.first->socket_handle() == handle) {
      pair.second->OnConnectionPending(pair.first);
      break;
    }
  }
}

void TlsDataRouterPosix::RemoveWatchedSocket(StreamSocketPosix* socket) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  const auto it = socket_mappings_.find(socket);
  if (it != socket_mappings_.end()) {
    socket_mappings_.erase(it);
  }
}

bool TlsDataRouterPosix::IsSocketWatched(StreamSocketPosix* socket) const {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  return socket_mappings_.find(socket) != socket_mappings_.end();
}

}  // namespace platform
}  // namespace openscreen
