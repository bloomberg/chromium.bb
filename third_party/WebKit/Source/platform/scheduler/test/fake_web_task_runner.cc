// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/fake_web_task_runner.h"

namespace blink {
namespace scheduler {

FakeWebTaskRunner::FakeWebTaskRunner() : time_(0.0) {}

FakeWebTaskRunner::~FakeWebTaskRunner() {}

void FakeWebTaskRunner::setTime(double new_time) {
  time_ = new_time;
}

void FakeWebTaskRunner::postTask(const WebTraceLocation&, Task*) {}

void FakeWebTaskRunner::postDelayedTask(const WebTraceLocation&,
                                        Task* task,
                                        double) {
  task_.reset(task);
}

bool FakeWebTaskRunner::runsTasksOnCurrentThread() {
  return true;
}

std::unique_ptr<WebTaskRunner> FakeWebTaskRunner::clone() {
  return nullptr;
}

double FakeWebTaskRunner::virtualTimeSeconds() const {
  return time_;
}

double FakeWebTaskRunner::monotonicallyIncreasingVirtualTimeSeconds() const {
  return time_;
}

SingleThreadTaskRunner* FakeWebTaskRunner::toSingleThreadTaskRunner() {
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
