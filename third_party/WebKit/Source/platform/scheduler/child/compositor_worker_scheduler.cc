// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/compositor_worker_scheduler.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/scheduler_helper.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"

namespace blink {
namespace scheduler {

CompositorWorkerScheduler::CompositorWorkerScheduler(
    base::Thread* thread,
    scoped_refptr<SchedulerTqmDelegate> main_task_runner)
    : WorkerScheduler(
          std::make_unique<WorkerSchedulerHelper>(main_task_runner)),
      thread_(thread) {}

CompositorWorkerScheduler::~CompositorWorkerScheduler() {}

void CompositorWorkerScheduler::Init() {}

scoped_refptr<WorkerTaskQueue> CompositorWorkerScheduler::DefaultTaskQueue() {
  return helper_->DefaultWorkerTaskQueue();
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorWorkerScheduler::DefaultTaskRunner() {
  return DefaultTaskQueue();
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
CompositorWorkerScheduler::IdleTaskRunner() {
  // TODO(flackr): This posts idle tasks as regular tasks. We need to create
  // an idle task runner with the semantics we want for the compositor thread
  // which runs them after the current frame has been drawn before the next
  // vsync. https://crbug.com/609532
  return make_scoped_refptr(
      new SingleThreadIdleTaskRunner(thread_->task_runner(), this));
}

bool CompositorWorkerScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

bool CompositorWorkerScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void CompositorWorkerScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_->AddTaskObserver(task_observer);
}

void CompositorWorkerScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_->RemoveTaskObserver(task_observer);
}

void CompositorWorkerScheduler::Shutdown() {}

void CompositorWorkerScheduler::OnIdleTaskPosted() {}

base::TimeTicks CompositorWorkerScheduler::WillProcessIdleTask() {
  // TODO(flackr): Return the next frame time as the deadline instead.
  // TODO(flackr): Ensure that oilpan GC does happen on the compositor thread
  // even though we will have no long idle periods. https://crbug.com/609531
  return base::TimeTicks::Now() + base::TimeDelta::FromMillisecondsD(16.7);
}

void CompositorWorkerScheduler::DidProcessIdleTask() {}

base::TimeTicks CompositorWorkerScheduler::NowTicks() {
  return base::TimeTicks::Now();
}

}  // namespace scheduler
}  // namespace blink
