// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/scheduler_tqm_delegate_impl.h"

#include <utility>

#include "base/run_loop.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<SchedulerTqmDelegateImpl> SchedulerTqmDelegateImpl::Create(
    base::MessageLoop* message_loop,
    std::unique_ptr<base::TickClock> time_source) {
  return make_scoped_refptr(
      new SchedulerTqmDelegateImpl(message_loop, std::move(time_source)));
}

SchedulerTqmDelegateImpl::SchedulerTqmDelegateImpl(
    base::MessageLoop* message_loop,
    std::unique_ptr<base::TickClock> time_source)
    : message_loop_(message_loop),
      message_loop_task_runner_(message_loop->task_runner()),
      time_source_(std::move(time_source)) {}

SchedulerTqmDelegateImpl::~SchedulerTqmDelegateImpl() {
  RestoreDefaultTaskRunner();
}

void SchedulerTqmDelegateImpl::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  message_loop_->SetTaskRunner(task_runner);
}

void SchedulerTqmDelegateImpl::RestoreDefaultTaskRunner() {
  if (base::MessageLoop::current() == message_loop_)
    message_loop_->SetTaskRunner(message_loop_task_runner_);
}

bool SchedulerTqmDelegateImpl::PostDelayedTask(
    const tracked_objects::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostDelayedTask(from_here, std::move(task),
                                                    delay);
}

bool SchedulerTqmDelegateImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostNonNestableDelayedTask(
      from_here, std::move(task), delay);
}

bool SchedulerTqmDelegateImpl::RunsTasksInCurrentSequence() const {
  return message_loop_task_runner_->RunsTasksInCurrentSequence();
}

bool SchedulerTqmDelegateImpl::IsNested() const {
  DCHECK(RunsTasksInCurrentSequence());
  return base::RunLoop::IsNestedOnCurrentThread();
}

void SchedulerTqmDelegateImpl::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  base::RunLoop::AddNestingObserverOnCurrentThread(observer);
}

void SchedulerTqmDelegateImpl::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  base::RunLoop::RemoveNestingObserverOnCurrentThread(observer);
}

base::TimeTicks SchedulerTqmDelegateImpl::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
