// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <condition_variable>

#include "platform/api/logging.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader() : NetworkReader(NetworkWaiter::Create()) {}

NetworkReader::NetworkReader(std::unique_ptr<NetworkWaiter> waiter)
    : waiter_(std::move(waiter)), is_running_(false) {}

NetworkReader::~NetworkReader() = default;

// TODO(rwkeane): Remove unsafe casts to UdpSocketPosix.
Error NetworkReader::WaitAndRead(Clock::duration timeout) {
  // Get the set of all sockets we care about. A different list than the
  // existing unordered_set is used to avoid race conditions with the method
  // using this new list.
  socket_deletion_block_.notify_all();
  std::vector<SocketHandle> socket_handles;
  socket_handles.reserve(sockets_.size());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& socket : sockets_) {
      UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
      socket_handles.emplace_back(read_socket->GetFd());
    }
  }

  // Wait for the sockets to find something interesting or for the timeout.
  auto changed_or_error =
      waiter_->AwaitSocketsReadable(socket_handles, timeout);
  if (changed_or_error.is_error()) {
    return changed_or_error.error();
  }

  // Process the results.
  socket_deletion_block_.notify_all();
  const std::vector<SocketHandle>& changed_handles = changed_or_error.value();
  if (changed_handles.empty()) {
    return Error::None();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (UdpSocket* socket : sockets_) {
      UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
      if (std::find(changed_handles.begin(), changed_handles.end(),
                    SocketHandle(read_socket->GetFd())) !=
          changed_handles.end()) {
        read_socket->ReceiveMessage();
      }
    }
  }

  return Error::None();
}

void NetworkReader::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  Clock::duration timeout = std::chrono::milliseconds(50);
  while (is_running_) {
    WaitAndRead(timeout);
  }
}

void NetworkReader::RequestStopSoon() {
  is_running_.store(false);
}

void NetworkReader::OnCreate(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  sockets_.push_back(socket);
}

void NetworkReader::OnDestroy(UdpSocket* socket) {
  OnDelete(socket);
}

void NetworkReader::OnDelete(UdpSocket* socket,
                             bool disable_locking_for_testing) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = std::find(sockets_.begin(), sockets_.end(), socket);
  if (it != sockets_.end()) {
    sockets_.erase(it);
    if (!disable_locking_for_testing) {
      // This code will allow us to block completion of the socket destructor
      // (and subsequent invalidation of pointers to this socket) until we no
      // longer are waiting on a SELECT(...) call to it, since we only signal
      // this condition variable's wait(...) to proceed outside of SELECT(...).
      socket_deletion_block_.wait(lock);
    }
  }
}

}  // namespace platform
}  // namespace openscreen
