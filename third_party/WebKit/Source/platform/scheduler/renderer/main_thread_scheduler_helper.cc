// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/main_thread_scheduler_helper.h"

#include "platform/scheduler/renderer/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

MainThreadSchedulerHelper::MainThreadSchedulerHelper(
    std::unique_ptr<TaskQueueManager> task_queue_manager,
    RendererSchedulerImpl* renderer_scheduler)
    : SchedulerHelper(std::move(task_queue_manager)),
      renderer_scheduler_(renderer_scheduler),
      default_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::kDefault)
                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::kControl)
                           .SetShouldNotifyObservers(false))) {
  InitDefaultQueues(default_task_queue_, control_task_queue_);
  task_queue_manager_->EnableCrashKeys("blink_scheduler_task_file_name",
                                       "blink_scheduler_task_function_name");
}

MainThreadSchedulerHelper::~MainThreadSchedulerHelper() {
  control_task_queue_->ShutdownTaskQueue();
  default_task_queue_->ShutdownTaskQueue();
}

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

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerHelper::NewTaskQueue(
    const MainThreadTaskQueue::QueueCreationParams& params) {
  return task_queue_manager_->CreateTaskQueue<MainThreadTaskQueue>(
      params.spec, params, renderer_scheduler_);
}

}  // namespace scheduler
}  // namespace blink
