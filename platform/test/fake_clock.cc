// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_clock.h"

#include "platform/api/logging.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace platform {

FakeClock::FakeClock(Clock::time_point start_time) {
  OSP_CHECK(!instance_) << "attempting to use multiple fake clocks!";
  instance_ = this;
  now_ = start_time;
}

FakeClock::~FakeClock() {
  OSP_CHECK(task_runners_.empty());
  instance_ = nullptr;
}

Clock::time_point FakeClock::now() noexcept {
  OSP_CHECK(instance_);
  return now_.load();
}

void FakeClock::Advance(Clock::duration delta) {
  Clock::time_point x = now_.load();
  while (!now_.compare_exchange_weak(x, x + delta)) {
    x = now_.load();
  }

  for (FakeTaskRunner* task_runner : task_runners_) {
    task_runner->RunTasksUntilIdle();
  }
}

void FakeClock::SubscribeToTimeChanges(FakeTaskRunner* task_runner) {
  OSP_CHECK(std::find(task_runners_.begin(), task_runners_.end(),
                      task_runner) == task_runners_.end());
  task_runners_.push_back(task_runner);
}

void FakeClock::UnsubscribeFromTimeChanges(FakeTaskRunner* task_runner) {
  auto it = std::find(task_runners_.begin(), task_runners_.end(), task_runner);
  OSP_CHECK(it != task_runners_.end());
  task_runners_.erase(it);
}

// static
FakeClock* FakeClock::instance_ = nullptr;

// static
std::atomic<Clock::time_point> FakeClock::now_{Clock::time_point{}};

}  // namespace platform
}  // namespace openscreen
