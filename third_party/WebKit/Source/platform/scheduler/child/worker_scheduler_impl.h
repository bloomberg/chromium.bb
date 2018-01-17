// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/scheduler/child/idle_canceled_delayed_task_sweeper.h"
#include "platform/scheduler/child/idle_helper.h"
#include "platform/scheduler/child/worker_metrics_helper.h"
#include "platform/scheduler/child/worker_scheduler.h"
#include "platform/scheduler/util/task_duration_metric_reporter.h"
#include "platform/scheduler/util/thread_load_tracker.h"
#include "platform/scheduler/util/thread_type.h"

namespace blink {
namespace scheduler {

class TaskQueueManager;

class PLATFORM_EXPORT WorkerSchedulerImpl : public WorkerScheduler,
                                            public IdleHelper::Delegate,
                                            public TaskTimeObserver {
 public:
  explicit WorkerSchedulerImpl(
      std::unique_ptr<TaskQueueManager> task_queue_manager);
  ~WorkerSchedulerImpl() override;

  // ChildScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;

  // WorkerScheduler implementation:
  scoped_refptr<WorkerTaskQueue> DefaultTaskQueue() override;
  void Init() override;
  void OnTaskCompleted(WorkerTaskQueue* worker_task_queue,
                       const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end,
                       base::Optional<base::TimeDelta> thread_time) override;

  // TaskTimeObserver implementation:
  void WillProcessTask(double start_time) override;
  void DidProcessTask(double start_time, double end_time) override;

  SchedulerHelper* GetSchedulerHelperForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

  void SetThreadType(ThreadType thread_type) override;

 protected:
  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override {}
  void OnIdlePeriodEnded() override {}
  void OnPendingTasksChanged(bool new_state) override {}

 private:
  void MaybeStartLongIdlePeriod();

  IdleHelper idle_helper_;
  IdleCanceledDelayedTaskSweeper idle_canceled_delayed_task_sweeper_;
  ThreadLoadTracker load_tracker_;
  bool initialized_;
  base::TimeTicks thread_start_time_;

  WorkerMetricsHelper worker_metrics_helper_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_
