// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager_delegate_for_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<TaskQueueManagerDelegateForTest>
TaskQueueManagerDelegateForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source) {
  return base::WrapRefCounted(
      new TaskQueueManagerDelegateForTest(task_runner, std::move(time_source)));
}

TaskQueueManagerDelegateForTest::TaskQueueManagerDelegateForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source)
    : task_runner_(task_runner), time_source_(std::move(time_source)) {}

TaskQueueManagerDelegateForTest::~TaskQueueManagerDelegateForTest() {}

bool TaskQueueManagerDelegateForTest::PostDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, std::move(task), delay);
}

bool TaskQueueManagerDelegateForTest::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                  delay);
}

bool TaskQueueManagerDelegateForTest::RunsTasksInCurrentSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

bool TaskQueueManagerDelegateForTest::IsNested() const {
  return false;
}

void TaskQueueManagerDelegateForTest::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {}

void TaskQueueManagerDelegateForTest::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {}

base::TimeTicks TaskQueueManagerDelegateForTest::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
