// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_
#define PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_

#include <unistd.h>

#include <atomic>
#include <mutex>  // NOLINT

#include "platform/impl/socket_handle_waiter.h"

namespace openscreen {
namespace platform {

class SocketHandleWaiterPosix : public SocketHandleWaiter {
 public:
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;

  SocketHandleWaiterPosix();
  ~SocketHandleWaiterPosix() override;

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

 protected:
  ErrorOr<std::vector<SocketHandleRef>> AwaitSocketsReadable(
      const std::vector<SocketHandleRef>& socket_fds,
      const Clock::duration& timeout) override;

 private:
  fd_set read_handles_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_
