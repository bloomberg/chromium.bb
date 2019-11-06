// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <condition_variable>

#include "platform/api/logging.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader(TaskRunner* task_runner)
    : NetworkReader(task_runner, NetworkWaiter::Create()) {}

NetworkReader::NetworkReader(TaskRunner* task_runner,
                             std::unique_ptr<NetworkWaiter> waiter)
    : waiter_(std::move(waiter)),
      task_runner_(task_runner),
      is_running_(false) {
  OSP_CHECK(task_runner_);
}

NetworkReader::~NetworkReader() = default;

Error NetworkReader::ReadRepeatedly(UdpSocket* socket, Callback callback) {
  socket->SetDeletionCallback(
      [this](UdpSocket* socket) { this->CancelReadForSocketDeletion(socket); });
  std::lock_guard<std::mutex> lock(mutex_);
  return !read_callbacks_.emplace(socket, std::move(callback)).second
             ? Error::Code::kIOFailure
             : Error::None();
}

Error NetworkReader::CancelRead(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  return read_callbacks_.erase(socket) != 0 ? Error::Code::kNone
                                            : Error::Code::kOperationInvalid;
}

Error NetworkReader::WaitAndRead(Clock::duration timeout) {
  // Get the set of all sockets we care about.
  socket_deletion_block_.notify_all();
  std::vector<UdpSocket*> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : read_callbacks_) {
      sockets.push_back(read.first);
    }
  }

  // Wait for the sockets to find something interesting or for the timeout.
  auto changed_or_error = waiter_->AwaitSocketsReadable(sockets, timeout);
  if (changed_or_error.is_error()) {
    return changed_or_error.error();
  }

  // Process the results.
  socket_deletion_block_.notify_all();
  Error error = Error::None();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (UdpSocket* read : changed_or_error.value()) {
      auto mapped_socket = read_callbacks_.find(read);
      if (mapped_socket == read_callbacks_.end()) {
        continue;
      }

      // TODO(rwkeane): Remove this unsafe cast.
      UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(read);
      ErrorOr<UdpPacket> read_packet = read_socket->ReceiveMessage();
      if (read_packet.is_error()) {
        error = read_packet.error();
        continue;
      }

      // Capture the UdpPacket by move into |arg| here to transfer the ownership
      // and avoid copying the UdpPacket. This move constructs the UdpPacket
      // inside of the lambda. Then the UdpPacket |arg| is passed by move to the
      // callback function |func|.
      auto executor = [arg = read_packet.MoveValue(),
                       func = mapped_socket->second]() mutable {
        func(std::move(arg));
      };

      task_runner_->PostTask(std::move(executor));
    }
  }

  return error;
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

void NetworkReader::CancelReadForSocketDeletion(UdpSocket* socket) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (read_callbacks_.erase(socket) != 0) {
    // This code will allow us to block completion of the socket destructor (and
    // subsequent invalidation of pointers to this socket) until we no longer
    // are waiting on a SELECT(...) call to it, since we only signal this
    // condition variable's wait(...) to proceed outside of SELECT(...).
    socket_deletion_block_.wait(lock);
  }
}

}  // namespace platform
}  // namespace openscreen
