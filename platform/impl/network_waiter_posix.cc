// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_waiter_posix.h"

#include <time.h>

#include <algorithm>
#include <vector>

#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkWaiterPosix::NetworkWaiterPosix() = default;

NetworkWaiterPosix::~NetworkWaiterPosix() = default;

ErrorOr<std::vector<SocketHandle>> NetworkWaiterPosix::AwaitSocketsReadable(
    const std::vector<SocketHandle>& socket_handles,
    const Clock::duration& timeout) {
  int max_fd = -1;
  FD_ZERO(&read_handles_);
  for (const SocketHandle& handle : socket_handles) {
    FD_SET(handle.fd, &read_handles_);
    max_fd = std::max(max_fd, handle.fd);
  }
  if (max_fd < 0) {
    return Error::Code::kIOFailure;
  }

  struct timeval tv = ToTimeval(timeout);
  // This value is set to 'max_fd + 1' by convention. For more information, see:
  // http://man7.org/linux/man-pages/man2/select.2.html
  int max_fd_to_watch = max_fd + 1;
  const int rv = select(max_fd_to_watch, &read_handles_, nullptr, nullptr, &tv);
  if (rv == -1) {
    // This is the case when an error condition is hit within the select(...)
    // command.
    return Error::Code::kIOFailure;
  } else if (rv == 0) {
    // This occurs when no sockets have a pending read.
    return Error::Code::kAgain;
  }

  std::vector<SocketHandle> changed_handles;
  for (const SocketHandle& handle : socket_handles) {
    if (FD_ISSET(handle.fd, &read_handles_)) {
      changed_handles.push_back(handle);
    }
  }

  return changed_handles;
}

// static
std::unique_ptr<NetworkWaiter> NetworkWaiter::Create() {
  return std::unique_ptr<NetworkWaiter>(new NetworkWaiterPosix());
}

// static
struct timeval NetworkWaiterPosix::ToTimeval(const Clock::duration& timeout) {
  struct timeval tv;
  const auto whole_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(timeout);
  tv.tv_sec = whole_seconds.count();
  tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                   timeout - whole_seconds)
                   .count();

  return tv;
}

}  // namespace platform
}  // namespace openscreen
