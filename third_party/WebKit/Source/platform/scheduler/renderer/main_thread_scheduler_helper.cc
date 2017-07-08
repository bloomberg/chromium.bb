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
          NewTaskQueue(MainThreadTaskQueue::QueueType::DEFAULT,
                       MainThreadTaskQueue::CreateSpecForType(
                           MainThreadTaskQueue::QueueType::DEFAULT)
                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueType::CONTROL,
                       MainThreadTaskQueue::CreateSpecForType(
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

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerHelper::NewTaskQueue(
    MainThreadTaskQueue::QueueType type,
    const TaskQueue::Spec& spec) {
  return task_queue_manager_->CreateTaskQueue<MainThreadTaskQueue>(
      spec, type, renderer_scheduler_);
}

}  // namespace scheduler
}  // namespace blink
