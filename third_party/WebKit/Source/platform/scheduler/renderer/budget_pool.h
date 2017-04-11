// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_

#include <unordered_set>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "platform/scheduler/base/lazy_now.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {
namespace scheduler {

class TaskQueue;
class BudgetPoolController;

// BudgetPool represents a group of task queues which share a limit
// on a resource. This limit applies when task queues are already throttled
// by TaskQueueThrottler.
class BLINK_PLATFORM_EXPORT BudgetPool {
 public:
  virtual ~BudgetPool();

  const char* Name() const;

  // Report task run time to the budget pool.
  virtual void RecordTaskRunTime(base::TimeTicks start_time,
                                 base::TimeTicks end_time) = 0;

  // Retuns earliest time (can be in the past) when the next task can run.
  virtual base::TimeTicks GetNextAllowedRunTime() = 0;

  // Returns true at a task can be run immediately at the given time.
  virtual bool HasEnoughBudgetToRun(base::TimeTicks now) = 0;

  // Returns state for tracing.
  virtual void AsValueInto(base::trace_event::TracedValue* state,
                           base::TimeTicks now) const = 0;

  // Adds |queue| to given pool. If the pool restriction does not allow
  // a task to be run immediately and |queue| is throttled, |queue| becomes
  // disabled.
  void AddQueue(base::TimeTicks now, TaskQueue* queue);

  // Removes |queue| from given pool. If it is throttled, it does not
  // become enabled immediately, but a call to |PumpThrottledTasks|
  // is scheduled.
  void RemoveQueue(base::TimeTicks now, TaskQueue* queue);

  // Enables this time budget pool. Queues from this pool will be
  // throttled based on their run time.
  void EnableThrottling(LazyNow* now);

  // Disables with time budget pool. Queues from this pool will not be
  // throttled based on their run time. A call to |PumpThrottledTasks|
  // will be scheduled to enable this queues back again and respect
  // timer alignment. Internal budget level will not regenerate with time.
  void DisableThrottling(LazyNow* now);

  bool IsThrottlingEnabled() const;

  // All queues should be removed before calling Close().
  void Close();

  // Block all associated queues and schedule them to run when appropriate.
  void BlockThrottledQueues(base::TimeTicks now);

 protected:
  BudgetPool(const char* name, BudgetPoolController* budget_pool_controller);

  const char* name_;  // NOT OWNED

  BudgetPoolController* budget_pool_controller_;

  std::unordered_set<TaskQueue*> associated_task_queues_;
  bool is_enabled_;
};

// CPUTimeBudgetPool represents a collection of task queues which share a limit
// on total cpu time.
class BLINK_PLATFORM_EXPORT CPUTimeBudgetPool : public BudgetPool {
 public:
  CPUTimeBudgetPool(const char* name,
                    BudgetPoolController* budget_pool_controller,
                    base::TimeTicks now);

  ~CPUTimeBudgetPool();

  // Set max budget level, base::nullopt represent absence of max level.
  // Max budget level prevents accumulating arbitrary large budgets when
  // page is inactive for a very long time.
  void SetMaxBudgetLevel(base::TimeTicks now,
                         base::Optional<base::TimeDelta> max_budget_level);

  // Set max throttling duration, base::nullopt represents absense of it.
  // Max throttling duration prevents page from being throttled for
  // a very long period after a single long task.
  void SetMaxThrottlingDelay(
      base::TimeTicks now,
      base::Optional<base::TimeDelta> max_throttling_delay);

  // Set minimal budget level required to run a task. If budget pool was
  // exhausted, it needs to accumulate at least |min_budget_to_run| time units
  // to unblock and run tasks again. When unblocked, it still can run tasks
  // when budget is positive but less than this level until being blocked
  // until being blocked when budget reaches zero.
  // This is needed for integration with WakeUpBudgetPool to prevent a situation
  // when wake-up happened but time budget pool allows only one task to run at
  // the moment.
  // It is recommended to use the same value for this and WakeUpBudgetPool's
  // wake-up window length.
  // NOTE: This does not have an immediate effect and does not call
  // BudgetPoolController::UnblockQueue.
  void SetMinBudgetLevelToRun(base::TimeTicks now,
                              base::TimeDelta min_budget_level_to_run);

  // Throttle task queues from this time budget pool if tasks are running
  // for more than |cpu_percentage| per cent of wall time.
  // This function does not affect internal time budget level.
  void SetTimeBudgetRecoveryRate(base::TimeTicks now, double cpu_percentage);

  // Increase budget level by given value. This function DOES NOT unblock
  // queues even if they are allowed to run with increased budget level.
  void GrantAdditionalBudget(base::TimeTicks now, base::TimeDelta budget_level);

  // Set callback which will be called every time when this budget pool
  // is throttled. Throttling duration (time until the queue is allowed
  // to run again) is passed as a parameter to callback.
  void SetReportingCallback(
      base::Callback<void(base::TimeDelta)> reporting_callback);

  // BudgetPool implementation:
  void RecordTaskRunTime(base::TimeTicks start_time,
                         base::TimeTicks end_time) final;
  bool HasEnoughBudgetToRun(base::TimeTicks now) final;
  base::TimeTicks GetNextAllowedRunTime() final;
  void AsValueInto(base::trace_event::TracedValue* state,
                   base::TimeTicks now) const final;

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskQueueThrottlerTest, CPUTimeBudgetPool);

  // Advances |last_checkpoint_| to |now| if needed and recalculates
  // budget level.
  void Advance(base::TimeTicks now);

  // Increase |current_budget_level_| to satisfy max throttling duration
  // condition if necessary.
  // Decrease |current_budget_level_| to satisfy max budget level
  // condition if necessary.
  void EnforceBudgetLevelRestrictions();

  // Max budget level which we can accrue.
  // Tasks will be allowed to run for this time before being throttled
  // after a very long period of inactivity.
  base::Optional<base::TimeDelta> max_budget_level_;
  // Max throttling delay places a lower limit on time budget level,
  // ensuring that one long task does not cause extremely long throttling.
  // Note that this is not a guarantee that every task will run
  // after desired run time + max throttling duration, but a guarantee
  // that at least one task will be run every max_throttling_delay.
  base::Optional<base::TimeDelta> max_throttling_delay_;
  // See CPUTimeBudgetPool::SetMinBudgetLevelToRun.
  base::TimeDelta min_budget_level_to_run_;

  base::TimeDelta current_budget_level_;
  base::TimeTicks last_checkpoint_;
  double cpu_percentage_;

  base::Callback<void(base::TimeDelta)> reporting_callback_;

  DISALLOW_COPY_AND_ASSIGN(CPUTimeBudgetPool);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_
