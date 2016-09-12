// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/lazy_scheduler_message_loop_delegate_for_tests.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<LazySchedulerMessageLoopDelegateForTests>
LazySchedulerMessageLoopDelegateForTests::Create() {
  return make_scoped_refptr(new LazySchedulerMessageLoopDelegateForTests());
}

LazySchedulerMessageLoopDelegateForTests::
    LazySchedulerMessageLoopDelegateForTests()
    : message_loop_(base::MessageLoop::current()),
      thread_id_(base::PlatformThread::CurrentId()),
      time_source_(base::MakeUnique<base::DefaultTickClock>()),
      pending_observer_(nullptr) {
  if (message_loop_)
    original_task_runner_ = message_loop_->task_runner();
}

LazySchedulerMessageLoopDelegateForTests::
    ~LazySchedulerMessageLoopDelegateForTests() {
  RestoreDefaultTaskRunner();
}

base::MessageLoop* LazySchedulerMessageLoopDelegateForTests::EnsureMessageLoop()
    const {
  if (message_loop_)
    return message_loop_;
  DCHECK(RunsTasksOnCurrentThread());
  message_loop_ = base::MessageLoop::current();
  DCHECK(message_loop_);
  original_task_runner_ = message_loop_->task_runner();
  if (pending_task_runner_)
    message_loop_->SetTaskRunner(std::move(pending_task_runner_));
  if (pending_observer_)
    message_loop_->AddNestingObserver(pending_observer_);
  return message_loop_;
}

void LazySchedulerMessageLoopDelegateForTests::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!HasMessageLoop()) {
    pending_task_runner_ = std::move(task_runner);
    return;
  }
  message_loop_->SetTaskRunner(std::move(task_runner));
}

void LazySchedulerMessageLoopDelegateForTests::RestoreDefaultTaskRunner() {
  if (HasMessageLoop() && base::MessageLoop::current() == message_loop_)
    message_loop_->SetTaskRunner(original_task_runner_);
}

bool LazySchedulerMessageLoopDelegateForTests::HasMessageLoop() const {
  return message_loop_ != nullptr;
}

bool LazySchedulerMessageLoopDelegateForTests::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  EnsureMessageLoop();
  return original_task_runner_->PostDelayedTask(from_here, task, delay);
}

bool LazySchedulerMessageLoopDelegateForTests::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  EnsureMessageLoop();
  return original_task_runner_->PostNonNestableDelayedTask(from_here, task,
                                                           delay);
}

bool LazySchedulerMessageLoopDelegateForTests::RunsTasksOnCurrentThread()
    const {
  return thread_id_ == base::PlatformThread::CurrentId();
}

bool LazySchedulerMessageLoopDelegateForTests::IsNested() const {
  return EnsureMessageLoop()->IsNested();
}

void LazySchedulerMessageLoopDelegateForTests::AddNestingObserver(
    base::MessageLoop::NestingObserver* observer) {
  if (!HasMessageLoop()) {
    pending_observer_ = observer;
    return;
  }
  message_loop_->AddNestingObserver(observer);
}

void LazySchedulerMessageLoopDelegateForTests::RemoveNestingObserver(
    base::MessageLoop::NestingObserver* observer) {
  if (!message_loop_ || message_loop_ != base::MessageLoop::current()) {
    DCHECK_EQ(pending_observer_, observer);
    pending_observer_ = nullptr;
    return;
  }
  message_loop_->RemoveNestingObserver(observer);
}

base::TimeTicks LazySchedulerMessageLoopDelegateForTests::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
