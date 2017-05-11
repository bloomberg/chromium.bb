// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/wake_up_budget_pool.h"

#include <cstdint>

#include "platform/scheduler/base/trace_helper.h"
#include "platform/scheduler/renderer/task_queue_throttler.h"

namespace blink {
namespace scheduler {

WakeUpBudgetPool::WakeUpBudgetPool(const char* name,
                                   BudgetPoolController* budget_pool_controller,
                                   base::TimeTicks now)
    : BudgetPool(name, budget_pool_controller), wakeups_per_second_(1) {}

WakeUpBudgetPool::~WakeUpBudgetPool() {}

QueueBlockType WakeUpBudgetPool::GetBlockType() const {
  return QueueBlockType::kNewTasksOnly;
}

void WakeUpBudgetPool::SetWakeUpRate(double wakeups_per_second) {
  wakeups_per_second_ = wakeups_per_second;
}

void WakeUpBudgetPool::SetWakeUpDuration(base::TimeDelta duration) {
  wakeup_duration_ = duration;
}

void WakeUpBudgetPool::RecordTaskRunTime(TaskQueue* queue,
                                         base::TimeTicks start_time,
                                         base::TimeTicks end_time) {
  budget_pool_controller_->UpdateQueueThrottlingState(end_time, queue);
}

base::Optional<base::TimeTicks> WakeUpBudgetPool::NextWakeUp() const {
  if (!last_wakeup_)
    return base::nullopt;
  // Subtract 1 microsecond to work with time alignment in task queue throttler.
  // This is needed due to alignment mechanism in task queue throttler --
  // whole seconds need to be aligned to the next second to deal with immediate
  // tasks correctly. By subtracting 1 microsecond we ensure that next wakeup
  // gets aligned to a correct time.
  return last_wakeup_.value() +
         base::TimeDelta::FromSeconds(1 / wakeups_per_second_) -
         base::TimeDelta::FromMicroseconds(1);
}

bool WakeUpBudgetPool::CanRunTasksAt(base::TimeTicks moment,
                                     bool is_wake_up) const {
  if (!last_wakeup_)
    return false;
  // |is_wake_up| flag means that we're in the beginning of the wake-up and
  // |OnWakeUp| has just been called. This is needed to support backwards
  // compability with old throttling mechanism (when |wakeup_duration| is zero)
  // and allow only one task to run.
  if (last_wakeup_ == moment && is_wake_up)
    return true;
  return moment < last_wakeup_.value() + wakeup_duration_;
}

base::TimeTicks WakeUpBudgetPool::GetNextAllowedRunTime(
    base::TimeTicks desired_run_time) const {
  if (!last_wakeup_)
    return desired_run_time;
  if (desired_run_time < last_wakeup_.value() + wakeup_duration_)
    return desired_run_time;
  return std::max(desired_run_time, NextWakeUp().value());
}

void WakeUpBudgetPool::OnQueueNextWakeUpChanged(
    TaskQueue* queue,
    base::TimeTicks now,
    base::TimeTicks desired_run_time) {
  budget_pool_controller_->UpdateQueueThrottlingState(now, queue);
}

void WakeUpBudgetPool::OnWakeUp(base::TimeTicks now) {
  last_wakeup_ = now;
}

void WakeUpBudgetPool::AsValueInto(base::trace_event::TracedValue* state,
                                   base::TimeTicks now) const {
  state->BeginDictionary(name_);

  state->SetString("name", name_);
  state->SetDouble("wakeups_per_second_rate", wakeups_per_second_);
  state->SetDouble("wakeup_duration_in_seconds", wakeup_duration_.InSecondsF());
  if (last_wakeup_) {
    state->SetDouble("last_wakeup_seconds_ago",
                     (now - last_wakeup_.value()).InSecondsF());
  }
  state->SetBoolean("is_enabled", is_enabled_);

  state->BeginArray("task_queues");
  for (TaskQueue* queue : associated_task_queues_) {
    state->AppendString(trace_helper::PointerToString(queue));
  }
  state->EndArray();

  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
