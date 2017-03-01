// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/time_domain.h"

#include <set>

#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"
#include "platform/scheduler/base/work_queue.h"

namespace blink {
namespace scheduler {

TimeDomain::TimeDomain(Observer* observer) : observer_(observer) {}

TimeDomain::~TimeDomain() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void TimeDomain::RegisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
}

void TimeDomain::UnregisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);

  CancelDelayedWork(queue);
}

void TimeDomain::ScheduleDelayedWork(
    internal::TaskQueueImpl* queue,
    internal::TaskQueueImpl::DelayedWakeUp wake_up,
    base::TimeTicks now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  DCHECK(queue->IsQueueEnabled());
  // We only want to store a single wakeup per queue, so we need to remove any
  // previously registered wake up for |queue|.
  if (queue->heap_handle().IsValid()) {
    DCHECK_NE(queue->scheduled_time_domain_wakeup(), base::TimeTicks());

    // O(log n)
    delayed_wakeup_queue_.ChangeKey(queue->heap_handle(), {wake_up, queue});
  } else {
    // O(log n)
    delayed_wakeup_queue_.insert({wake_up, queue});
  }

  queue->set_scheduled_time_domain_wakeup(wake_up.time);

  // If |queue| is the first wakeup then request the wakeup.
  if (delayed_wakeup_queue_.min().queue == queue)
    RequestWakeupAt(now, wake_up.time);

  if (observer_)
    observer_->OnTimeDomainHasDelayedWork(queue);
}

void TimeDomain::OnQueueHasImmediateWork(internal::TaskQueueImpl* queue) {
  if (observer_)
    observer_->OnTimeDomainHasImmediateWork(queue);
}

void TimeDomain::CancelDelayedWork(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);

  // If no wakeup has been requested then bail out.
  if (!queue->heap_handle().IsValid())
    return;

  DCHECK_NE(queue->scheduled_time_domain_wakeup(), base::TimeTicks());
  DCHECK(!delayed_wakeup_queue_.empty());
  base::TimeTicks prev_first_wakeup = delayed_wakeup_queue_.min().wake_up.time;

  // O(log n)
  delayed_wakeup_queue_.erase(queue->heap_handle());

  if (delayed_wakeup_queue_.empty()) {
    CancelWakeupAt(prev_first_wakeup);
  } else if (prev_first_wakeup != delayed_wakeup_queue_.min().wake_up.time) {
    CancelWakeupAt(prev_first_wakeup);
    RequestWakeupAt(Now(), delayed_wakeup_queue_.min().wake_up.time);
  }
}

void TimeDomain::WakeupReadyDelayedQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multimap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wakeup.
  while (!delayed_wakeup_queue_.empty() &&
         delayed_wakeup_queue_.min().wake_up.time <= lazy_now->Now()) {
    internal::TaskQueueImpl* queue = delayed_wakeup_queue_.min().queue;
    base::Optional<internal::TaskQueueImpl::DelayedWakeUp> next_wake_up =
        queue->WakeUpForDelayedWork(lazy_now);

    if (next_wake_up) {
      // O(log n)
      delayed_wakeup_queue_.ReplaceMin({*next_wake_up, queue});
      queue->set_scheduled_time_domain_wakeup(next_wake_up->time);
    } else {
      // O(log n)
      delayed_wakeup_queue_.pop();
      DCHECK_EQ(queue->scheduled_time_domain_wakeup(), base::TimeTicks());
    }
  }
}

bool TimeDomain::NextScheduledRunTime(base::TimeTicks* out_time) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_queue_.empty())
    return false;

  *out_time = delayed_wakeup_queue_.min().wake_up.time;
  return true;
}

bool TimeDomain::NextScheduledTaskQueue(TaskQueue** out_task_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_queue_.empty())
    return false;

  *out_task_queue = delayed_wakeup_queue_.min().queue;
  return true;
}

void TimeDomain::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetInteger("registered_delay_count", delayed_wakeup_queue_.size());
  if (!delayed_wakeup_queue_.empty()) {
    base::TimeDelta delay = delayed_wakeup_queue_.min().wake_up.time - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
