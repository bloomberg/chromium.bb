// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter_thread.h"

namespace openscreen {
namespace platform {

SocketHandleWaiterThread::SocketHandleWaiterThread()
    : thread_(&SocketHandleWaiterPosix::RunUntilStopped, &waiter_) {}

SocketHandleWaiterThread::~SocketHandleWaiterThread() {
  waiter_.RequestStopSoon();
  thread_.join();
}

}  // namespace platform
}  // namespace openscreen
