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

TimeDomain::TimeDomain() {}

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
  // We only want to store a single wake-up per queue, so we need to remove any
  // previously registered wake up for |queue|.
  if (queue->heap_handle().IsValid()) {
    DCHECK(queue->scheduled_time_domain_wake_up());

    // O(log n)
    delayed_wake_up_queue_.ChangeKey(queue->heap_handle(), {wake_up, queue});
  } else {
    // O(log n)
    delayed_wake_up_queue_.insert({wake_up, queue});
  }

  queue->SetScheduledTimeDomainWakeUp(wake_up.time);

  // If |queue| is the first wake-up then request the wake-up.
  if (delayed_wake_up_queue_.Min().queue == queue)
    RequestWakeUpAt(now, wake_up.time);
}

void TimeDomain::CancelDelayedWork(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);

  // If no wake-up has been requested then bail out.
  if (!queue->heap_handle().IsValid())
    return;

  DCHECK(queue->scheduled_time_domain_wake_up());
  DCHECK(!delayed_wake_up_queue_.empty());
  base::TimeTicks prev_first_wake_up =
      delayed_wake_up_queue_.Min().wake_up.time;

  // O(log n)
  delayed_wake_up_queue_.erase(queue->heap_handle());

  if (delayed_wake_up_queue_.empty()) {
    CancelWakeUpAt(prev_first_wake_up);
  } else if (prev_first_wake_up != delayed_wake_up_queue_.Min().wake_up.time) {
    CancelWakeUpAt(prev_first_wake_up);
    RequestWakeUpAt(Now(), delayed_wake_up_queue_.Min().wake_up.time);
  }
}

void TimeDomain::WakeUpReadyDelayedQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multimap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wake-up.
  while (!delayed_wake_up_queue_.empty() &&
         delayed_wake_up_queue_.Min().wake_up.time <= lazy_now->Now()) {
    internal::TaskQueueImpl* queue = delayed_wake_up_queue_.Min().queue;
    base::Optional<internal::TaskQueueImpl::DelayedWakeUp> next_wake_up =
        queue->WakeUpForDelayedWork(lazy_now);

    if (next_wake_up) {
      // O(log n)
      delayed_wake_up_queue_.ReplaceMin({*next_wake_up, queue});
      queue->SetScheduledTimeDomainWakeUp(next_wake_up->time);
    } else {
      // O(log n)
      delayed_wake_up_queue_.Pop();
      DCHECK(!queue->scheduled_time_domain_wake_up());
    }
  }
}

bool TimeDomain::NextScheduledRunTime(base::TimeTicks* out_time) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wake_up_queue_.empty())
    return false;

  *out_time = delayed_wake_up_queue_.Min().wake_up.time;
  return true;
}

bool TimeDomain::NextScheduledTaskQueue(
    internal::TaskQueueImpl** out_task_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wake_up_queue_.empty())
    return false;

  *out_task_queue = delayed_wake_up_queue_.Min().queue;
  return true;
}

void TimeDomain::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetInteger("registered_delay_count", delayed_wake_up_queue_.size());
  if (!delayed_wake_up_queue_.empty()) {
    base::TimeDelta delay = delayed_wake_up_queue_.Min().wake_up.time - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
