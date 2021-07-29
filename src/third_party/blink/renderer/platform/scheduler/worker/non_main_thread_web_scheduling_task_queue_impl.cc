// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_web_scheduling_task_queue_impl.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

NonMainThreadWebSchedulingTaskQueueImpl::
    NonMainThreadWebSchedulingTaskQueueImpl(
        scoped_refptr<NonMainThreadTaskQueue> task_queue)
    : task_runner_(
          task_queue->CreateTaskRunner(TaskType::kExperimentalWebScheduling)),
      task_queue_(std::move(task_queue)) {}

void NonMainThreadWebSchedulingTaskQueueImpl::SetPriority(
    WebSchedulingPriority priority) {
  task_queue_->SetWebSchedulingPriority(priority);
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadWebSchedulingTaskQueueImpl::GetTaskRunner() {
  return task_runner_;
}

}  // namespace scheduler
}  // namespace blink
