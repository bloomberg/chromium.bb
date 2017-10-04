// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_helper.h"

#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/child/worker_task_queue.h"

namespace blink {
namespace scheduler {

WorkerSchedulerHelper::WorkerSchedulerHelper(
    scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate)
    : SchedulerHelper(task_queue_manager_delegate),
      default_task_queue_(NewTaskQueue(TaskQueue::Spec("worker_default_tq")
                                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(NewTaskQueue(TaskQueue::Spec("worker_control_tq")
                                           .SetShouldNotifyObservers(false))) {
  InitDefaultQueues(default_task_queue_, control_task_queue_);
}

WorkerSchedulerHelper::~WorkerSchedulerHelper() {
  control_task_queue_->UnregisterTaskQueue();
  default_task_queue_->UnregisterTaskQueue();
}

scoped_refptr<WorkerTaskQueue> WorkerSchedulerHelper::DefaultWorkerTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<TaskQueue> WorkerSchedulerHelper::DefaultTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<WorkerTaskQueue> WorkerSchedulerHelper::ControlWorkerTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<TaskQueue> WorkerSchedulerHelper::ControlTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<WorkerTaskQueue> WorkerSchedulerHelper::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  return task_queue_manager_->CreateTaskQueue<WorkerTaskQueue>(spec);
}

}  // namespace scheduler
}  // namespace blink
