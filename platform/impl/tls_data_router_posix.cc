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
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::OnConnectionDestroyed(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
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
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

}  // namespace platform
}  // namespace openscreen
