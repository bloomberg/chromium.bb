// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/child/compositor_metrics_helper.h"
#include "platform/scheduler/child/worker_scheduler.h"
#include "platform/scheduler/util/task_duration_metric_reporter.h"
#include "platform/scheduler/util/thread_type.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"

namespace base {
class Thread;
}

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT CompositorWorkerScheduler
    : public WorkerScheduler,
      public SingleThreadIdleTaskRunner::Delegate {
 public:
  CompositorWorkerScheduler(
      base::Thread* thread,
      std::unique_ptr<TaskQueueManager> task_queue_manager);

  ~CompositorWorkerScheduler() override;

  // WorkerScheduler:
  scoped_refptr<WorkerTaskQueue> DefaultTaskQueue() override;
  void Init() override;
  void OnTaskCompleted(WorkerTaskQueue* worker_task_queue,
                       const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end,
                       base::Optional<base::TimeDelta> thread_time) override;
  void SetThreadType(ThreadType thread_type) override;

  // ChildScheduler:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> IdleTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;

  // SingleThreadIdleTaskRunner::Delegate:
  void OnIdleTaskPosted() override;
  base::TimeTicks WillProcessIdleTask() override;
  void DidProcessIdleTask() override;
  base::TimeTicks NowTicks() override;

 private:
  base::Thread* thread_;

  CompositorMetricsHelper compositor_metrics_helper_;

  DISALLOW_COPY_AND_ASSIGN(CompositorWorkerScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
