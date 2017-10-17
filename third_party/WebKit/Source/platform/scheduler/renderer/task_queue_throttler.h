// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_

#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/cancelable_closure_holder.h"
#include "platform/scheduler/base/time_domain.h"
#include "platform/scheduler/renderer/budget_pool.h"
#include "platform/scheduler/renderer/cpu_time_budget_pool.h"
#include "platform/scheduler/renderer/wake_up_budget_pool.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {
namespace scheduler {

class BudgetPool;
class RendererSchedulerImpl;
class ThrottledTimeDomain;
class CPUTimeBudgetPool;
class WakeUpBudgetPool;

// kNewTasksOnly prevents new tasks from running (old tasks can run normally),
// kAllTasks block queue completely.
// kAllTasks-type block always blocks the queue completely.
// kNewTasksOnly-type block does nothing when queue is already blocked by
// kAllTasks, and overrides previous kNewTasksOnly block if any, which may
// unblock some tasks.
enum class QueueBlockType { kAllTasks, kNewTasksOnly };

// Interface for BudgetPool to interact with TaskQueueThrottler.
class PLATFORM_EXPORT BudgetPoolController {
 public:
  virtual ~BudgetPoolController() {}

  // To be used by BudgetPool only, use BudgetPool::{Add,Remove}Queue
  // methods instead.
  virtual void AddQueueToBudgetPool(TaskQueue* queue,
                                    BudgetPool* budget_pool) = 0;
  virtual void RemoveQueueFromBudgetPool(TaskQueue* queue,
                                         BudgetPool* budget_pool) = 0;

  // Deletes the budget pool.
  virtual void UnregisterBudgetPool(BudgetPool* budget_pool) = 0;

  // Ensure that an appropriate type of the fence is installed and schedule
  // a pump for this queue when needed.
  virtual void UpdateQueueThrottlingState(base::TimeTicks now,
                                          TaskQueue* queue) = 0;

  // Returns true if the |queue| is throttled (i.e. added to TaskQueueThrottler
  // and throttling is not disabled).
  virtual bool IsThrottled(TaskQueue* queue) const = 0;
};

// The job of the TaskQueueThrottler is to control when tasks posted on
// throttled queues get run. The TaskQueueThrottler:
// - runs throttled tasks once per second,
// - controls time budget for task queues grouped in CPUTimeBudgetPools.
//
// This is done by disabling throttled queues and running
// a special "heart beat" function |PumpThrottledTasks| which when run
// temporarily enables throttled queues and inserts a fence to ensure tasks
// posted from a throttled task run next time the queue is pumped.
//
// Of course the TaskQueueThrottler isn't the only sub-system that wants to
// enable or disable queues. E.g. RendererSchedulerImpl also does this for
// policy reasons. To prevent the systems from fighting, clients of
// TaskQueueThrottler must use SetQueueEnabled rather than calling the function
// directly on the queue.
//
// There may be more than one system that wishes to throttle a queue (e.g.
// renderer suspension vs tab level suspension) so the TaskQueueThrottler keeps
// a count of the number of systems that wish a queue to be throttled.
// See IncreaseThrottleRefCount & DecreaseThrottleRefCount.
//
// This class is main-thread only.
class PLATFORM_EXPORT TaskQueueThrottler : public TaskQueue::Observer,
                                           public BudgetPoolController {
 public:
  explicit TaskQueueThrottler(RendererSchedulerImpl* renderer_scheduler);

  ~TaskQueueThrottler() override;

  // TaskQueue::Observer implementation:
  void OnQueueNextWakeUpChanged(TaskQueue* queue,
                                base::TimeTicks wake_up) override;

  // BudgetPoolController implementation:
  void AddQueueToBudgetPool(TaskQueue* queue, BudgetPool* budget_pool) override;
  void RemoveQueueFromBudgetPool(TaskQueue* queue,
                                 BudgetPool* budget_pool) override;
  void UnregisterBudgetPool(BudgetPool* budget_pool) override;
  void UpdateQueueThrottlingState(base::TimeTicks now,
                                  TaskQueue* queue) override;
  bool IsThrottled(TaskQueue* queue) const override;

  // Increments the throttled refcount and causes |task_queue| to be throttled
  // if its not already throttled.
  void IncreaseThrottleRefCount(TaskQueue* task_queue);

  // If the refcouint is non-zero it's decremented.  If the throttled refcount
  // becomes zero then |task_queue| is unthrottled.  If the refcount was already
  // zero this function does nothing.
  void DecreaseThrottleRefCount(TaskQueue* task_queue);

  // Removes |task_queue| from |queue_details| and from appropriate budget pool.
  void UnregisterTaskQueue(TaskQueue* task_queue);

  // Disable throttling for all queues, this setting takes precedence over
  // all other throttling settings. Designed to be used when a global event
  // disabling throttling happens (e.g. audio is playing).
  void DisableThrottling();

  // Enable back global throttling.
  void EnableThrottling();

  const ThrottledTimeDomain* time_domain() const { return time_domain_.get(); }

  // TODO(altimin): Remove it.
  static base::TimeTicks AlignedThrottledRunTime(
      base::TimeTicks unthrottled_runtime);

  const scoped_refptr<TaskQueue>& task_queue() const {
    return control_task_queue_;
  }

  // Returned object is owned by |TaskQueueThrottler|.
  CPUTimeBudgetPool* CreateCPUTimeBudgetPool(const char* name);
  WakeUpBudgetPool* CreateWakeUpBudgetPool(const char* name);

  // Accounts for given task for cpu-based throttling needs.
  void OnTaskRunTimeReported(TaskQueue* task_queue,
                             base::TimeTicks start_time,
                             base::TimeTicks end_time);

  void AsValueInto(base::trace_event::TracedValue* state,
                   base::TimeTicks now) const;
 private:
  struct Metadata {
    Metadata() : throttling_ref_count(0) {}

    size_t throttling_ref_count;

    std::unordered_set<BudgetPool*> budget_pools;
  };
  using TaskQueueMap = std::unordered_map<TaskQueue*, Metadata>;

  void PumpThrottledTasks();

  // Note |unthrottled_runtime| might be in the past. When this happens we
  // compute the delay to the next runtime based on now rather than
  // unthrottled_runtime.
  void MaybeSchedulePumpThrottledTasks(const base::Location& from_here,
                                       base::TimeTicks now,
                                       base::TimeTicks runtime);

  // Return next possible time when queue is allowed to run in accordance
  // with throttling policy.
  base::TimeTicks GetNextAllowedRunTime(TaskQueue* queue,
                                        base::TimeTicks desired_run_time);

  bool CanRunTasksAt(TaskQueue* queue, base::TimeTicks moment, bool is_wake_up);

  base::Optional<base::TimeTicks> GetTimeTasksCanRunUntil(
      TaskQueue* queue,
      base::TimeTicks now,
      bool is_wake_up) const;

  void MaybeDeleteQueueMetadata(TaskQueueMap::iterator it);

  void UpdateQueueThrottlingStateInternal(base::TimeTicks now,
                                          TaskQueue* queue,
                                          bool is_wake_up);

  base::Optional<QueueBlockType> GetQueueBlockType(base::TimeTicks now,
                                                   TaskQueue* queue);

  TaskQueueMap queue_details_;
  base::Callback<void(TaskQueue*, base::TimeTicks)>
      forward_immediate_work_callback_;
  scoped_refptr<TaskQueue> control_task_queue_;
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED
  base::TickClock* tick_clock_;                // NOT OWNED
  std::unique_ptr<ThrottledTimeDomain> time_domain_;

  CancelableClosureHolder pump_throttled_tasks_closure_;
  base::Optional<base::TimeTicks> pending_pump_throttled_tasks_runtime_;
  bool allow_throttling_;

  std::unordered_map<BudgetPool*, std::unique_ptr<BudgetPool>> budget_pools_;

  base::WeakPtrFactory<TaskQueueThrottler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_
