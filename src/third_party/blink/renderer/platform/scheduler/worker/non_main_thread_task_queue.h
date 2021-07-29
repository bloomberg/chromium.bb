// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_

#include "base/task/sequence_manager/task_queue_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {
namespace scheduler {

class NonMainThreadSchedulerImpl;

class PLATFORM_EXPORT NonMainThreadTaskQueue
    : public base::sequence_manager::TaskQueue {
 public:
  // TODO(kraynov): Consider options to remove TaskQueueImpl reference here.
  NonMainThreadTaskQueue(
      std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
      const Spec& spec,
      NonMainThreadSchedulerImpl* non_main_thread_scheduler);
  ~NonMainThreadTaskQueue() override;

  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now);

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) {
    return TaskQueue::CreateTaskRunner(static_cast<int>(task_type));
  }

  // This method returns the default task runner with task type kTaskTypeNone
  // and is mostly used for tests. For most use cases, you'll want a more
  // specific task runner and should use the 'CreateTaskRunner' method and pass
  // the desired task type.
  const scoped_refptr<base::SingleThreadTaskRunner>&
  GetTaskRunnerWithDefaultTaskType() const {
    return task_runner();
  }

  void SetWebSchedulingPriority(WebSchedulingPriority priority);

 private:
  void OnWebSchedulingPriorityChanged();

  // Not owned.
  NonMainThreadSchedulerImpl* non_main_thread_scheduler_;

  // |web_scheduling_priority_| is the priority of the task queue within the web
  // scheduling API. This priority is used to determine the task queue priority.
  absl::optional<WebSchedulingPriority> web_scheduling_priority_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_
