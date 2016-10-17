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

  // If present remove |task_queue| from delayed_wakeup_multimap_.
  // O(log n)
  QueueToDelayedWakeupMultimapIteratorMap::iterator it =
      queue_to_delayed_wakeup_multimap_iterator_map_.find(queue);

  if (it == queue_to_delayed_wakeup_multimap_iterator_map_.end())
    return;

  // O(1) amortized.
  delayed_wakeup_multimap_.erase(it->second);
  queue_to_delayed_wakeup_multimap_iterator_map_.erase(it);
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

  // If present remove |task_queue| from delayed_wakeup_multimap_.
  // O(log n)
  QueueToDelayedWakeupMultimapIteratorMap::iterator it =
      queue_to_delayed_wakeup_multimap_iterator_map_.find(queue);

  if (it == queue_to_delayed_wakeup_multimap_iterator_map_.end())
    return;

  base::TimeTicks destination_now = destination_time_domain->Now();
  destination_time_domain->ScheduleDelayedWork(queue, it->second->first,
                                               destination_now);

  // O(1) amortized.
  delayed_wakeup_multimap_.erase(it->second);
  queue_to_delayed_wakeup_multimap_iterator_map_.erase(it);
}

void TimeDomain::ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                                     base::TimeTicks delayed_run_time,
                                     base::TimeTicks now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // We only want to store a single wakeup per queue, so we need to remove any
  // previously registered wake up for |queue|.
  QueueToDelayedWakeupMultimapIteratorMap::iterator it =
      queue_to_delayed_wakeup_multimap_iterator_map_.find(queue);

  if (it != queue_to_delayed_wakeup_multimap_iterator_map_.end())
    delayed_wakeup_multimap_.erase(it->second);

  if (delayed_wakeup_multimap_.empty() ||
      delayed_run_time < delayed_wakeup_multimap_.begin()->first) {
    base::TimeDelta delay = std::max(base::TimeDelta(), delayed_run_time - now);
    RequestWakeup(now, delay);
  }

  if (it != queue_to_delayed_wakeup_multimap_iterator_map_.end()) {
    // TODO(alexclarke): Use C++17 extract & insert to more efficiently modify
    // |queue_to_delayed_wakeup_multimap_iterator_map_|.
    it->second = delayed_wakeup_multimap_.insert({delayed_run_time, queue});
  } else {
    // Insert the wakeup and store the map iterator for later convenience.
    queue_to_delayed_wakeup_multimap_iterator_map_.insert(
        {queue, delayed_wakeup_multimap_.insert({delayed_run_time, queue})});
  }

  if (observer_)
    observer_->OnTimeDomainHasDelayedWork(queue);
}

void TimeDomain::RegisterAsUpdatableTaskQueue(internal::TaskQueueImpl* queue) {
  {
    base::AutoLock lock(newly_updatable_lock_);
    newly_updatable_.push_back(queue);
  }
  if (observer_)
    observer_->OnTimeDomainHasImmediateWork(queue);
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
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now->Now())
      break;

    internal::TaskQueueImpl* queue = next_wakeup->second;

    // O(1) amortized.
    delayed_wakeup_multimap_.erase(next_wakeup);
    // O(log n).
    queue_to_delayed_wakeup_multimap_iterator_map_.erase(queue);

    queue->WakeUpForDelayedWork(lazy_now);
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
