// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_waiter_thread.h"

namespace openscreen {
namespace platform {

NetworkWaiterThread::NetworkWaiterThread()
    : thread_(&NetworkWaiterPosix::RunUntilStopped, &network_waiter_) {}

NetworkWaiterThread::~NetworkWaiterThread() {
  network_waiter_.RequestStopSoon();
  thread_.join();
}

}  // namespace platform
}  // namespace openscreen
