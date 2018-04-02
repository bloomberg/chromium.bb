// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/worker/compositor_thread_scheduler.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/scheduler_helper.h"

namespace blink {
namespace scheduler {

CompositorWorkerScheduler::CompositorWorkerScheduler(
    base::Thread* thread,
    std::unique_ptr<TaskQueueManager> task_queue_manager)
    : WorkerScheduler(
          std::make_unique<WorkerSchedulerHelper>(std::move(task_queue_manager),
                                                  this)),
      thread_(thread) {}

CompositorWorkerScheduler::~CompositorWorkerScheduler() = default;

scoped_refptr<WorkerTaskQueue> CompositorWorkerScheduler::DefaultTaskQueue() {
  return helper_->DefaultWorkerTaskQueue();
}

void CompositorWorkerScheduler::Init() {}

void CompositorWorkerScheduler::OnTaskCompleted(
    WorkerTaskQueue* worker_task_queue,
    const TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  compositor_metrics_helper_.RecordTaskMetrics(worker_task_queue, task, start,
                                               end, thread_time);
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
  return base::MakeRefCounted<SingleThreadIdleTaskRunner>(
      thread_->task_runner(), this);
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorWorkerScheduler::IPCTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
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
