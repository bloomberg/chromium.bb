// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_TASK_QUEUE_H_

#include "platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

class WorkerScheduler;

class PLATFORM_EXPORT WorkerTaskQueue : public TaskQueue {
 public:
  WorkerTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                  const Spec& spec,
                  WorkerScheduler* worker_scheduler);
  ~WorkerTaskQueue() override;

  void OnTaskCompleted(const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end,
                       base::Optional<base::TimeDelta> thread_time);

 private:
  // Not owned.
  WorkerScheduler* worker_scheduler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_TASK_QUEUE_H_
