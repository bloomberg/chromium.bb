// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_WAITER_H_
#define PLATFORM_API_NETWORK_WAITER_H_

#include <vector>

#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {

// The class responsible for calling platform-level method to watch UDP sockets
// for available read data. Reading from these sockets is handled at a higher
// layer.
class NetworkWaiter {
 public:
  // Creates a new NetworkWaiter instance.
  static std::unique_ptr<NetworkWaiter> Create();

  virtual ~NetworkWaiter() = default;

  // Waits until data is available to read in one of the provided sockets or the
  // provided timeout has passed - whichever is first. If any sockets have read
  // data available, they are returned. Else, an error is returned.
  virtual ErrorOr<std::vector<UdpSocket*>> AwaitSocketsReadable(
      const std::vector<UdpSocket*>& sockets,
      const Clock::duration& timeout) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_WAITER_H_
