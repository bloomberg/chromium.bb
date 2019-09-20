// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_WAITER_THREAD_H_
#define PLATFORM_IMPL_NETWORK_WAITER_THREAD_H_

#include <thread>

#include "platform/impl/network_waiter_posix.h"

namespace openscreen {
namespace platform {

// This is the class responsible for handling the threading associated with
// a network waiter. More specifically, when this object is created, it
// starts a thread on which NetworkWaiter's RunUntilStopped method is called,
// and upon destruction it calls NetworkWaiter's RequestStopSoon method and
// joins the thread it created, blocking until the NetworkWaiter's operation
// completes.
// TODO(rwkeane): Move this class to osp/demo.
class NetworkWaiterThread {
 public:
  NetworkWaiterThread();
  ~NetworkWaiterThread();

  NetworkWaiter* network_waiter() { return &network_waiter_; }

 private:
  NetworkWaiterPosix network_waiter_;
  std::thread thread_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkWaiterThread);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_WAITER_THREAD_H_
