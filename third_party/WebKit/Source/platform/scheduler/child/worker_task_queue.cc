// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_task_queue.h"

#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/child/worker_scheduler.h"

namespace blink {
namespace scheduler {

WorkerTaskQueue::WorkerTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                                 const TaskQueue::Spec& spec,
                                 WorkerScheduler* worker_scheduler)
    : TaskQueue(std::move(impl), spec), worker_scheduler_(worker_scheduler) {
  if (GetTaskQueueImpl()) {
    // TaskQueueImpl may be null for tests.
    GetTaskQueueImpl()->SetOnTaskCompletedHandler(
        base::Bind(&WorkerTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

WorkerTaskQueue::~WorkerTaskQueue() = default;

void WorkerTaskQueue::OnTaskCompleted(
    const TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  // |worker_scheduler_| can be nullptr in tests.
  if (worker_scheduler_)
    worker_scheduler_->OnTaskCompleted(this, task, start, end, thread_time);
}

}  // namespace scheduler
}  // namespace blink
