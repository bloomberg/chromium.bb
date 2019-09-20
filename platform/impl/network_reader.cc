// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <functional>

#include "platform/api/logging.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader(NetworkWaiter* waiter) : waiter_(waiter) {}

NetworkReader::~NetworkReader() {
  waiter_->UnsubscribeAll(this);
}

void NetworkReader::ProcessReadyHandle(SocketHandleRef handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  // NOTE: Because sockets_ is expected to remain small, the performance here
  // is better than using an unordered_set.
  for (UdpSocket* socket : sockets_) {
    UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
    if (read_socket->GetHandle() == handle) {
      read_socket->ReceiveMessage();
      break;
    }
  }
}

void NetworkReader::OnCreate(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  sockets_.push_back(socket);
  UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
  waiter_->Subscribe(this, std::cref(read_socket->GetHandle()));
}

void NetworkReader::OnDestroy(UdpSocket* socket) {
  OnDelete(socket);
}

void NetworkReader::OnDelete(UdpSocket* socket,
                             bool disable_locking_for_testing) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = std::find(sockets_.begin(), sockets_.end(), socket);
    if (it != sockets_.end()) {
      sockets_.erase(it);
    }
  }

  UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
  waiter_->OnHandleDeletion(this, std::cref(read_socket->GetHandle()),
                            disable_locking_for_testing);
}

}  // namespace platform
}  // namespace openscreen
