// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter_posix.h"

#include <time.h>

#include <algorithm>
#include <vector>

#include "platform/base/error.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/timeval_posix.h"
#include "platform/impl/udp_socket_posix.h"
#include "util/logging.h"

namespace openscreen {
namespace platform {

SocketHandleWaiterPosix::SocketHandleWaiterPosix() = default;

SocketHandleWaiterPosix::~SocketHandleWaiterPosix() = default;

ErrorOr<std::vector<SocketHandleWaiterPosix::SocketHandleRef>>
SocketHandleWaiterPosix::AwaitSocketsReadable(
    const std::vector<SocketHandleRef>& socket_handles,
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

  std::vector<SocketHandleRef> changed_handles;
  for (const SocketHandleRef& handle : socket_handles) {
    if (FD_ISSET(handle.get().fd, &read_handles_)) {
      changed_handles.push_back(handle);
    }
  }

  return changed_handles;
}

void SocketHandleWaiterPosix::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  constexpr Clock::duration kHandleReadyTimeout = std::chrono::milliseconds(50);
  while (is_running_) {
    ProcessHandles(kHandleReadyTimeout);
  }
}

void SocketHandleWaiterPosix::RequestStopSoon() {
  is_running_.store(false);
}

}  // namespace platform
}  // namespace openscreen
