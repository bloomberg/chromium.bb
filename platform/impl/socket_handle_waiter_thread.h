// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SOCKET_HANDLE_WAITER_THREAD_H_
#define PLATFORM_IMPL_SOCKET_HANDLE_WAITER_THREAD_H_

#include <thread>

#include "platform/impl/socket_handle_waiter_posix.h"

namespace openscreen {
namespace platform {

// This is the class responsible for handling the threading associated with
// a socket handle waiter. More specifically, when this object is created, it
// starts a thread on which NetworkWaiter's RunUntilStopped method is called,
// and upon destruction it calls NetworkWaiter's RequestStopSoon method and
// joins the thread it created, blocking until the NetworkWaiter's operation
// completes.
// TODO(rwkeane): Move this class to osp/demo.
class SocketHandleWaiterThread {
 public:
  SocketHandleWaiterThread();
  ~SocketHandleWaiterThread();

  SocketHandleWaiter* socket_handle_waiter() { return &waiter_; }

  OSP_DISALLOW_COPY_AND_ASSIGN(SocketHandleWaiterThread);

 private:
  SocketHandleWaiterPosix waiter_;
  std::thread thread_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SOCKET_HANDLE_WAITER_THREAD_H_
