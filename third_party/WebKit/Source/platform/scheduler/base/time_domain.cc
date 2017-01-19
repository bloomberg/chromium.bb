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

  {
    base::AutoLock lock(has_incoming_immediate_work_lock_);
    has_incoming_immediate_work_.erase(queue);
  }

  // If no wakeup has been requested then bail out.
  if (!queue->heap_handle().IsValid())
    return;

  DCHECK_NE(queue->scheduled_time_domain_wakeup(), base::TimeTicks());

  // O(log n)
  delayed_wakeup_queue_.erase(queue->heap_handle());
}

void TimeDomain::MigrateQueue(internal::TaskQueueImpl* queue,
                              TimeDomain* destination_time_domain) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  DCHECK(destination_time_domain);

  // Make sure we remember to update |queue| if it's got incoming immediate
  // work.
  bool has_incoming_immediate_work;
  {
    base::AutoLock lock(has_incoming_immediate_work_lock_);
    has_incoming_immediate_work = has_incoming_immediate_work_.erase(queue);
  }
  if (has_incoming_immediate_work) {
    base::AutoLock lock(
        destination_time_domain->has_incoming_immediate_work_lock_);
    destination_time_domain->has_incoming_immediate_work_.insert(queue);
  }

  // If no wakeup has been requested then bail out.
  if (!queue->heap_handle().IsValid())
    return;

  base::TimeTicks wake_up_time = queue->scheduled_time_domain_wakeup();
  DCHECK_NE(wake_up_time, base::TimeTicks());

  // O(log n)
  delayed_wakeup_queue_.erase(queue->heap_handle());

  base::TimeTicks destination_now = destination_time_domain->Now();
  destination_time_domain->ScheduleDelayedWork(queue, wake_up_time,
                                               destination_now);
}

void TimeDomain::ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                                     base::TimeTicks delayed_run_time,
                                     base::TimeTicks now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // We only want to store a single wakeup per queue, so we need to remove any
  // previously registered wake up for |queue|.
  if (queue->heap_handle().IsValid()) {
    DCHECK_NE(queue->scheduled_time_domain_wakeup(), base::TimeTicks());

    // O(log n)
    delayed_wakeup_queue_.ChangeKey(queue->heap_handle(),
                                    {delayed_run_time, queue});
  } else {
    // O(log n)
    delayed_wakeup_queue_.insert({delayed_run_time, queue});
  }

  queue->set_scheduled_time_domain_wakeup(delayed_run_time);

  // If |queue| is the first wakeup then request the wakeup.
  if (delayed_wakeup_queue_.min().queue == queue) {
    base::TimeDelta delay = std::max(base::TimeDelta(), delayed_run_time - now);
    RequestWakeup(now, delay);
  }

  if (observer_)
    observer_->OnTimeDomainHasDelayedWork(queue);
}

void TimeDomain::OnQueueHasIncomingImmediateWork(
    internal::TaskQueueImpl* queue) {
  {
    base::AutoLock lock(has_incoming_immediate_work_lock_);
    has_incoming_immediate_work_.insert(queue);
  }

  if (observer_)
    observer_->OnTimeDomainHasImmediateWork(queue);
}

void TimeDomain::UpdateWorkQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Move any ready delayed tasks into the Incoming queues.
  WakeupReadyDelayedQueues(lazy_now);

  std::set<internal::TaskQueueImpl*> queues_to_reload_if_empty;

  {
    base::AutoLock lock(has_incoming_immediate_work_lock_);
    std::swap(queues_to_reload_if_empty, has_incoming_immediate_work_);
  }

  for (internal::TaskQueueImpl* queue : queues_to_reload_if_empty)
    queue->ReloadImmediateWorkQueueIfEmpty();
}

void TimeDomain::WakeupReadyDelayedQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multipmap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wakeup.
  while (!delayed_wakeup_queue_.empty() &&
         delayed_wakeup_queue_.min().time <= lazy_now->Now()) {
    internal::TaskQueueImpl* queue = delayed_wakeup_queue_.min().queue;
    // O(log n)
    delayed_wakeup_queue_.pop();

    queue->WakeUpForDelayedWork(lazy_now);
  }
}

bool TimeDomain::NextScheduledRunTime(base::TimeTicks* out_time) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_queue_.empty())
    return false;

  *out_time = delayed_wakeup_queue_.min().time;
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
  {
    base::AutoLock lock(has_incoming_immediate_work_lock_);
    state->BeginArray("has_incoming_immediate_work");
    for (internal::TaskQueueImpl* queue : has_incoming_immediate_work_)
      state->AppendString(queue->GetName());
    state->EndArray();
  }
  state->SetInteger("registered_delay_count", delayed_wakeup_queue_.size());
  if (!delayed_wakeup_queue_.empty()) {
    base::TimeDelta delay = delayed_wakeup_queue_.min().time - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
