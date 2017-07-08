// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_HELPER_H_

#include "platform/scheduler/child/scheduler_helper.h"

#include "platform/scheduler/child/worker_task_queue.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT WorkerSchedulerHelper : public SchedulerHelper {
 public:
  explicit WorkerSchedulerHelper(
      scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate);
  ~WorkerSchedulerHelper() override;

  scoped_refptr<WorkerTaskQueue> NewTaskQueue(const TaskQueue::Spec& spec);

  scoped_refptr<WorkerTaskQueue> DefaultWorkerTaskQueue();
  scoped_refptr<WorkerTaskQueue> ControlWorkerTaskQueue();

 protected:
  scoped_refptr<TaskQueue> DefaultTaskQueue() override;
  scoped_refptr<TaskQueue> ControlTaskQueue() override;

 protected:
  const scoped_refptr<WorkerTaskQueue> default_task_queue_;
  const scoped_refptr<WorkerTaskQueue> control_task_queue_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_HELPER_H_
