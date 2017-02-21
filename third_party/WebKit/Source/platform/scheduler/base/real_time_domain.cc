// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/real_time_domain.h"

#include "base/bind.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"

namespace blink {
namespace scheduler {

RealTimeDomain::RealTimeDomain(const char* tracing_category)
    : TimeDomain(nullptr),
      tracing_category_(tracing_category),
      task_queue_manager_(nullptr) {}

RealTimeDomain::RealTimeDomain(TimeDomain::Observer* observer,
                               const char* tracing_category)
    : TimeDomain(observer),
      tracing_category_(tracing_category),
      task_queue_manager_(nullptr) {}

RealTimeDomain::~RealTimeDomain() {}

void RealTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManager* task_queue_manager) {
  task_queue_manager_ = task_queue_manager;
  DCHECK(task_queue_manager_);
}

LazyNow RealTimeDomain::CreateLazyNow() const {
  return task_queue_manager_->CreateLazyNow();
}

base::TimeTicks RealTimeDomain::Now() const {
  return task_queue_manager_->delegate()->NowTicks();
}

void RealTimeDomain::RequestWakeupAt(base::TimeTicks now,
                                     base::TimeTicks run_time) {
  // NOTE this is only called if the scheduled runtime is sooner than any
  // previously scheduled runtime, or there is no (outstanding) previously
  // scheduled runtime.
  task_queue_manager_->MaybeScheduleDelayedWork(FROM_HERE, this, now, run_time);
}

void RealTimeDomain::CancelWakeupAt(base::TimeTicks run_time) {
  task_queue_manager_->CancelDelayedWork(this, run_time);
}

base::Optional<base::TimeDelta> RealTimeDomain::DelayTillNextTask(
    LazyNow* lazy_now) {
  base::TimeTicks next_run_time;
  if (!NextScheduledRunTime(&next_run_time))
    return base::nullopt;

  base::TimeTicks now = lazy_now->Now();
  if (now >= next_run_time)
    return base::TimeDelta();  // Makes DoWork post an immediate continuation.

  base::TimeDelta delay = next_run_time - now;
  TRACE_EVENT1(tracing_category_, "RealTimeDomain::DelayTillNextTask",
               "delay_ms", delay.InMillisecondsF());

  // The next task is sometime in the future. DoWork will make sure it gets
  // run at the right time.
  return delay;
}

void RealTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}
}  // namespace scheduler
}  // namespace blink
