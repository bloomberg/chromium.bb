// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_task_runner.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

FakeTaskRunner::FakeTaskRunner(FakeClock* clock) : clock_(clock) {
  OSP_CHECK(clock_);
  clock_->SubscribeToTimeChanges(this);
}

FakeTaskRunner::~FakeTaskRunner() {
  clock_->UnsubscribeFromTimeChanges(this);
}

void FakeTaskRunner::RunTasksUntilIdle() {
  const auto current_time = FakeClock::now();
  const auto end_of_range = delayed_tasks_.upper_bound(current_time);
  for (auto it = delayed_tasks_.begin(); it != end_of_range; ++it) {
    ready_to_run_tasks_.push_back(std::move(it->second));
  }
  delayed_tasks_.erase(delayed_tasks_.begin(), end_of_range);

  std::vector<Task> running_tasks;
  running_tasks.swap(ready_to_run_tasks_);
  for (Task& task : running_tasks) {
    // Move the task out of the vector and onto the stack so that it destroys
    // just after being run. This helps catch "dangling reference/pointer" bugs.
    std::move(task)();
  }
}

void FakeTaskRunner::PostPackagedTask(Task task) {
  ready_to_run_tasks_.push_back(std::move(task));
}

void FakeTaskRunner::PostPackagedTaskWithDelay(Task task,
                                               Clock::duration delay) {
  delayed_tasks_.emplace(
      std::make_pair(FakeClock::now() + delay, std::move(task)));
}

}  // namespace platform
}  // namespace openscreen
