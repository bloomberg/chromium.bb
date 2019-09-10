// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_WAITER_POSIX_H_
#define PLATFORM_IMPL_NETWORK_WAITER_POSIX_H_

#include <sys/select.h>
#include <unistd.h>

#include <atomic>
#include <mutex>  // NOLINT

#include "platform/api/network_waiter.h"

namespace openscreen {
namespace platform {

class NetworkWaiterPosix : public NetworkWaiter {
 public:
  NetworkWaiterPosix();
  ~NetworkWaiterPosix();
  ErrorOr<std::vector<SocketHandle>> AwaitSocketsReadable(
      const std::vector<SocketHandle>& socket_handles,
      const Clock::duration& timeout);

  // TODO(rwkeane): Move this to a platform-specific util library.
  static struct timeval ToTimeval(const Clock::duration& timeout);

 private:
  fd_set read_handles_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_WAITER_POSIX_H_
