// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "platform/scheduler/base/task_queue_manager_delegate_for_test.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<SchedulerTqmDelegateForTest> SchedulerTqmDelegateForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source) {
  return base::WrapRefCounted(
      new SchedulerTqmDelegateForTest(task_runner, std::move(time_source)));
}

SchedulerTqmDelegateForTest::SchedulerTqmDelegateForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source)
    : task_runner_(
          TaskQueueManagerDelegateForTest::Create(task_runner,
                                                  std::move(time_source))) {}

SchedulerTqmDelegateForTest::~SchedulerTqmDelegateForTest() {}

void SchedulerTqmDelegateForTest::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  default_task_runner_ = std::move(task_runner);
}

void SchedulerTqmDelegateForTest::RestoreDefaultTaskRunner() {
  default_task_runner_ = nullptr;
}

bool SchedulerTqmDelegateForTest::PostDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, std::move(task), delay);
}

bool SchedulerTqmDelegateForTest::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                  delay);
}

bool SchedulerTqmDelegateForTest::RunsTasksInCurrentSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

bool SchedulerTqmDelegateForTest::IsNested() const {
  return task_runner_->IsNested();
}

void SchedulerTqmDelegateForTest::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {}

void SchedulerTqmDelegateForTest::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {}

base::TimeTicks SchedulerTqmDelegateForTest::NowTicks() {
  return task_runner_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
