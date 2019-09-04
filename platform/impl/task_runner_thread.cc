// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/task_runner_thread.h"

namespace openscreen {
namespace platform {

TaskRunnerThread::TaskRunnerThread(ClockNowFunctionPtr clock_func)
    : task_runner_(clock_func),
      thread_(&TaskRunnerImpl::RunUntilStopped, &task_runner_) {}

TaskRunnerThread::~TaskRunnerThread() {
  task_runner_.RequestStopSoon();
  thread_.join();
}

}  // namespace platform
}  // namespace openscreen
