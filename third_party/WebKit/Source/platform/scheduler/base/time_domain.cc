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
  UnregisterAsUpdatableTaskQueue(queue);

  // We need to remove |task_queue| from delayed_wakeup_multimap_ which is a
  // little awkward since it's keyed by time. O(n) running time.
  for (DelayedWakeupMultimap::iterator iter = delayed_wakeup_multimap_.begin();
       iter != delayed_wakeup_multimap_.end();) {
    if (iter->second == queue) {
      // O(1) amortized.
      iter = delayed_wakeup_multimap_.erase(iter);
    } else {
      iter++;
    }
  }
}

void TimeDomain::MigrateQueue(internal::TaskQueueImpl* queue,
                              TimeDomain* destination_time_domain) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  DCHECK(destination_time_domain);

  // Make sure we remember to update |queue| if it's got incoming immediate
  // work.
  if (UnregisterAsUpdatableTaskQueue(queue))
    destination_time_domain->updatable_queue_set_.insert(queue);

  base::TimeTicks destination_now = destination_time_domain->Now();
  // We need to remove |task_queue| from delayed_wakeup_multimap_ which is a
  // little awkward since it's keyed by time. O(n) running time.
  for (DelayedWakeupMultimap::iterator iter = delayed_wakeup_multimap_.begin();
       iter != delayed_wakeup_multimap_.end();) {
    if (iter->second == queue) {
      destination_time_domain->ScheduleDelayedWork(queue, iter->first,
                                                   destination_now);
      // O(1) amortized.
      iter = delayed_wakeup_multimap_.erase(iter);
    } else {
      iter++;
    }
  }
}

void TimeDomain::ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                                     base::TimeTicks delayed_run_time,
                                     base::TimeTicks now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty() ||
      delayed_run_time < delayed_wakeup_multimap_.begin()->first) {
    base::TimeDelta delay = std::max(base::TimeDelta(), delayed_run_time - now);
    RequestWakeup(now, delay);
  }

  delayed_wakeup_multimap_.insert(std::make_pair(delayed_run_time, queue));
  if (observer_)
    observer_->OnTimeDomainHasDelayedWork();
}

void TimeDomain::CancelDelayedWork(internal::TaskQueueImpl* queue,
                                   base::TimeTicks delayed_run_time) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  auto iterpair = delayed_wakeup_multimap_.equal_range(delayed_run_time);
  for (auto it = iterpair.first; it != iterpair.second; ++it) {
    if (it->second == queue) {
      base::TimeTicks prev_first_wakeup =
          delayed_wakeup_multimap_.begin()->first;

      // Note we only erase the entry corresponding to |queue|, there might be
      // other queues that happen to require a wake up at |delayed_run_time|
      // which we must respect.
      delayed_wakeup_multimap_.erase(it);

      // If |delayed_wakeup_multimap_| is now empty, there's nothing to be done.
      // Sadly the base TaskRunner does and some OSes don't support cancellation
      // so we can't cancel something requested with RequestWakeup.
      if (delayed_wakeup_multimap_.empty())
        break;

      base::TimeTicks first_wakeup = delayed_wakeup_multimap_.begin()->first;

      // If the first_wakeup hasn't changed we don't need to do anything, the
      // wakeup was already requested.
      if (first_wakeup == prev_first_wakeup)
        break;

      // The first wakeup has changed, we need to re-schedule.
      base::TimeTicks now = Now();
      base::TimeDelta delay = std::max(base::TimeDelta(), first_wakeup - now);
      RequestWakeup(now, delay);
      break;
    }
  }
}

void TimeDomain::RegisterAsUpdatableTaskQueue(internal::TaskQueueImpl* queue) {
  {
    base::AutoLock lock(newly_updatable_lock_);
    newly_updatable_.push_back(queue);
  }
  if (observer_)
    observer_->OnTimeDomainHasImmediateWork();
}

bool TimeDomain::UnregisterAsUpdatableTaskQueue(
    internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  bool was_updatable = updatable_queue_set_.erase(queue) != 0;

  base::AutoLock lock(newly_updatable_lock_);
  // Remove all copies of |queue| from |newly_updatable_|.
  for (size_t i = 0; i < newly_updatable_.size();) {
    if (newly_updatable_[i] == queue) {
      // Move last element into slot #i and then compact.
      newly_updatable_[i] = newly_updatable_.back();
      newly_updatable_.pop_back();
      was_updatable = true;
    } else {
      i++;
    }
  }
  return was_updatable;
}

void TimeDomain::UpdateWorkQueues(LazyNow lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Move any ready delayed tasks into the Incoming queues.
  WakeupReadyDelayedQueues(&lazy_now);

  MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();

  std::set<internal::TaskQueueImpl*>::iterator iter =
      updatable_queue_set_.begin();
  while (iter != updatable_queue_set_.end()) {
    std::set<internal::TaskQueueImpl*>::iterator queue_it = iter++;
    internal::TaskQueueImpl* queue = *queue_it;

    // Update the queue and remove from the set if subsequent updates are not
    // required.
    if (!queue->MaybeUpdateImmediateWorkQueues())
      updatable_queue_set_.erase(queue_it);
  }
}

void TimeDomain::MoveNewlyUpdatableQueuesIntoUpdatableQueueSet() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(newly_updatable_lock_);
  while (!newly_updatable_.empty()) {
    updatable_queue_set_.insert(newly_updatable_.back());
    newly_updatable_.pop_back();
  }
}

void TimeDomain::WakeupReadyDelayedQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multipmap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wakeup.
  std::set<internal::TaskQueueImpl*> dedup_set;
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now->Now())
      break;
    // A queue could have any number of delayed tasks pending so it's worthwhile
    // deduping calls to UpdateDelayedWorkQueue since it takes a lock.
    // NOTE the order in which these are called matters since the order
    // in which EnqueueTaskLocks is called is respected when choosing which
    // queue to execute a task from.
    if (dedup_set.insert(next_wakeup->second).second) {
      next_wakeup->second->WakeUpForDelayedWork(lazy_now);
    }
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
}

void TimeDomain::ClearExpiredWakeups() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  LazyNow lazy_now(CreateLazyNow());
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now.Now())
      break;
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
}

bool TimeDomain::NextScheduledRunTime(base::TimeTicks* out_time) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty())
    return false;

  *out_time = delayed_wakeup_multimap_.begin()->first;
  return true;
}

bool TimeDomain::NextScheduledTaskQueue(TaskQueue** out_task_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty())
    return false;

  *out_task_queue = delayed_wakeup_multimap_.begin()->second;
  return true;
}

void TimeDomain::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->BeginArray("updatable_queue_set");
  for (auto* queue : updatable_queue_set_)
    state->AppendString(queue->GetName());
  state->EndArray();
  state->SetInteger("registered_delay_count", delayed_wakeup_multimap_.size());
  if (!delayed_wakeup_multimap_.empty()) {
    base::TimeDelta delay = delayed_wakeup_multimap_.begin()->first - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
