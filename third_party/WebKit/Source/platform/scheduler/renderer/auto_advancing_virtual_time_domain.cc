// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"

#include "platform/scheduler/child/scheduler_helper.h"

namespace blink {
namespace scheduler {

AutoAdvancingVirtualTimeDomain::AutoAdvancingVirtualTimeDomain(
    base::TimeTicks initial_time,
    SchedulerHelper* helper)
    : VirtualTimeDomain(initial_time),
      task_starvation_count_(0),
      max_task_starvation_count_(0),
      can_advance_virtual_time_(true),
      observer_(nullptr),
      helper_(helper) {
  helper_->AddTaskObserver(this);
}

AutoAdvancingVirtualTimeDomain::~AutoAdvancingVirtualTimeDomain() {
  helper_->RemoveTaskObserver(this);
}

base::Optional<base::TimeDelta>
AutoAdvancingVirtualTimeDomain::DelayTillNextTask(LazyNow* lazy_now) {
  base::TimeTicks run_time;
  if (!can_advance_virtual_time_ || !NextScheduledRunTime(&run_time))
    return base::nullopt;

  if (MaybeAdvanceVirtualTime(run_time)) {
    task_starvation_count_ = 0;
    return base::TimeDelta();  // Makes DoWork post an immediate continuation.
  }

  return base::nullopt;
}

void AutoAdvancingVirtualTimeDomain::RequestWakeUpAt(base::TimeTicks now,
                                                     base::TimeTicks run_time) {
  // Avoid posting pointless DoWorks.  I.e. if the time domain has more then one
  // scheduled wake up then we don't need to do anything.
  if (can_advance_virtual_time_ && NumberOfScheduledWakeUps() == 1u)
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::CancelWakeUpAt(base::TimeTicks run_time) {
  // We ignore this because RequestWakeUpAt doesn't post a delayed task.
}

void AutoAdvancingVirtualTimeDomain::SetObserver(Observer* observer) {
  observer_ = observer;
}

void AutoAdvancingVirtualTimeDomain::SetCanAdvanceVirtualTime(
    bool can_advance_virtual_time) {
  can_advance_virtual_time_ = can_advance_virtual_time;
  if (can_advance_virtual_time_)
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::SetMaxVirtualTimeTaskStarvationCount(
    int max_task_starvation_count) {
  max_task_starvation_count_ = max_task_starvation_count;
  if (max_task_starvation_count_ == 0)
    task_starvation_count_ = 0;
}

void AutoAdvancingVirtualTimeDomain::SetVirtualTimeFence(
    base::TimeTicks virtual_time_fence) {
  DCHECK_GE(virtual_time_fence, virtual_time_fence);
  virtual_time_fence_ = virtual_time_fence;
  if (!requested_next_virtual_time_.is_null())
    MaybeAdvanceVirtualTime(requested_next_virtual_time_);
}

bool AutoAdvancingVirtualTimeDomain::MaybeAdvanceVirtualTime(
    base::TimeTicks new_virtual_time) {
  // If set, don't advance past the end of |virtual_time_fence_|.
  if (!virtual_time_fence_.is_null() &&
      new_virtual_time > virtual_time_fence_) {
    requested_next_virtual_time_ = new_virtual_time;
    new_virtual_time = virtual_time_fence_;
  } else {
    requested_next_virtual_time_ = base::TimeTicks();
  }

  if (new_virtual_time <= Now())
    return false;

  AdvanceTo(new_virtual_time);

  if (observer_)
    observer_->OnVirtualTimeAdvanced();

  return true;
}

const char* AutoAdvancingVirtualTimeDomain::GetName() const {
  return "AutoAdvancingVirtualTimeDomain";
}

void AutoAdvancingVirtualTimeDomain::WillProcessTask(
    const base::PendingTask& pending_task) {}

void AutoAdvancingVirtualTimeDomain::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (max_task_starvation_count_ == 0 ||
      ++task_starvation_count_ < max_task_starvation_count_) {
    return;
  }

  // Delayed tasks are being excessively starved, so allow virtual time to
  // advance.
  base::TimeTicks run_time;
  if (NextScheduledRunTime(&run_time) && MaybeAdvanceVirtualTime(run_time))
    task_starvation_count_ = 0;
}

AutoAdvancingVirtualTimeDomain::Observer::Observer() = default;

AutoAdvancingVirtualTimeDomain::Observer::~Observer() = default;

}  // namespace scheduler
}  // namespace blink
