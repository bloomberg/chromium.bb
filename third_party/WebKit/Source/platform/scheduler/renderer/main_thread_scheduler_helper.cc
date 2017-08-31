// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/main_thread_scheduler_helper.h"

#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

MainThreadSchedulerHelper::MainThreadSchedulerHelper(
    scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate,
    RendererSchedulerImpl* renderer_scheduler)
    : SchedulerHelper(task_queue_manager_delegate),
      renderer_scheduler_(renderer_scheduler),
      default_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::DEFAULT)
                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::CONTROL)
                           .SetShouldNotifyObservers(false))) {
  InitDefaultQueues(default_task_queue_, control_task_queue_);
}

MainThreadSchedulerHelper::~MainThreadSchedulerHelper() {}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerHelper::DefaultMainThreadTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<TaskQueue> MainThreadSchedulerHelper::DefaultTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerHelper::ControlMainThreadTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<TaskQueue> MainThreadSchedulerHelper::ControlTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerHelper::BestEffortMainThreadTaskQueue() {
  if (!best_effort_task_queue_) {
    best_effort_task_queue_ =
        NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                         MainThreadTaskQueue::QueueType::BEST_EFFORT)
                         .SetShouldMonitorQuiescence(true));
    best_effort_task_queue_->SetQueuePriority(TaskQueue::BEST_EFFORT_PRIORITY);
  }
  return best_effort_task_queue_;
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerHelper::NewTaskQueue(
    const MainThreadTaskQueue::QueueCreationParams& params) {
  return task_queue_manager_->CreateTaskQueue<MainThreadTaskQueue>(
      params.spec, params, renderer_scheduler_);
}

}  // namespace scheduler
}  // namespace blink
