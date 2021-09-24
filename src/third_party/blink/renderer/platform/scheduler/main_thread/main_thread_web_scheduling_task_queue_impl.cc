// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_web_scheduling_task_queue_impl.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

MainThreadWebSchedulingTaskQueueImpl::WebSchedulingTaskRunner::
    WebSchedulingTaskRunner(
        scoped_refptr<base::SingleThreadTaskRunner> immediate_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> delayed_task_runner)
    : immediate_task_runner_(std::move(immediate_task_runner)),
      delayed_task_runner_(std::move(delayed_task_runner)) {}

bool MainThreadWebSchedulingTaskQueueImpl::WebSchedulingTaskRunner::
    PostDelayedTask(const base::Location& location,
                    base::OnceClosure task,
                    base::TimeDelta delay) {
  return GetTaskRunnerForDelay(delay)->PostDelayedTask(location,
                                                       std::move(task), delay);
}

bool MainThreadWebSchedulingTaskQueueImpl::WebSchedulingTaskRunner::
    PostNonNestableDelayedTask(const base::Location& location,
                               base::OnceClosure task,
                               base::TimeDelta delay) {
  return GetTaskRunnerForDelay(delay)->PostNonNestableDelayedTask(
      location, std::move(task), delay);
}

bool MainThreadWebSchedulingTaskQueueImpl::WebSchedulingTaskRunner::
    RunsTasksInCurrentSequence() const {
  DCHECK_EQ(immediate_task_runner_->RunsTasksInCurrentSequence(),
            delayed_task_runner_->RunsTasksInCurrentSequence());
  return immediate_task_runner_->RunsTasksInCurrentSequence();
}

base::SingleThreadTaskRunner* MainThreadWebSchedulingTaskQueueImpl::
    WebSchedulingTaskRunner::GetTaskRunnerForDelay(base::TimeDelta delay) {
  return delay > base::TimeDelta() ? delayed_task_runner_.get()
                                   : immediate_task_runner_.get();
}

MainThreadWebSchedulingTaskQueueImpl::MainThreadWebSchedulingTaskQueueImpl(
    base::WeakPtr<MainThreadTaskQueue> immediate_task_queue,
    base::WeakPtr<MainThreadTaskQueue> delayed_task_queue)
    : task_runner_(base::MakeRefCounted<WebSchedulingTaskRunner>(
          immediate_task_queue->CreateTaskRunner(
              TaskType::kExperimentalWebScheduling),
          delayed_task_queue->CreateTaskRunner(
              TaskType::kExperimentalWebScheduling))),
      immediate_task_queue_(std::move(immediate_task_queue)),
      delayed_task_queue_(std::move(delayed_task_queue)) {}

MainThreadWebSchedulingTaskQueueImpl::~MainThreadWebSchedulingTaskQueueImpl() {
  if (immediate_task_queue_)
    immediate_task_queue_->OnWebSchedulingTaskQueueDestroyed();
  if (delayed_task_queue_)
    delayed_task_queue_->OnWebSchedulingTaskQueueDestroyed();
}

void MainThreadWebSchedulingTaskQueueImpl::SetPriority(
    WebSchedulingPriority priority) {
  if (immediate_task_queue_)
    immediate_task_queue_->SetWebSchedulingPriority(priority);
  if (delayed_task_queue_)
    delayed_task_queue_->SetWebSchedulingPriority(priority);
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadWebSchedulingTaskQueueImpl::GetTaskRunner() {
  return task_runner_;
}

}  // namespace scheduler
}  // namespace blink
