// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/ack_tracker.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/time/tick_clock.h"
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

namespace {

// All times are in milliseconds.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  // Note this value is set to 1 to work in conjunction with a hack in
  // AckTracker::Track.
  1,

  // Initial delay.  The interpretation of this value depends on
  // always_use_initial_delay.  It's either how long we wait between
  // requests before backoff starts, or how much we delay the first request
  // after backoff starts.
  60 * 1000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request, -1
  // for no maximum.
  60 * 10 * 1000,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // If true, we always use a delay of initial_delay_ms, even before
  // we've seen num_errors_to_ignore errors.  Otherwise, initial_delay_ms
  // is the first delay once we start exponential backoff.
  //
  // So if we're ignoring 1 error, we'll see (N, N, Nm, Nm^2, ...) if true,
  // and (0, 0, N, Nm, ...) when false, where N is initial_backoff_ms and
  // m is multiply_factor, assuming we've already seen one success.
  true,
};

scoped_ptr<net::BackoffEntry> CreateDefaultBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return scoped_ptr<net::BackoffEntry>(new net::BackoffEntry(policy));
}

}  // namespace

AckTracker::Delegate::~Delegate() {
}

AckTracker::Entry::Entry(scoped_ptr<net::BackoffEntry> backoff,
                         const ObjectIdSet& ids)
    : backoff(backoff.Pass()), ids(ids) {
}

AckTracker::Entry::~Entry() {
}

AckTracker::AckTracker(base::TickClock* tick_clock, Delegate* delegate)
    : create_backoff_entry_callback_(base::Bind(&CreateDefaultBackoffEntry)),
      tick_clock_(tick_clock),
      delegate_(delegate) {
  DCHECK(tick_clock_);
  DCHECK(delegate_);
}

AckTracker::~AckTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Clear();
}

void AckTracker::Clear() {
  DCHECK(thread_checker_.CalledOnValidThread());

  timer_.Stop();
  STLDeleteValues(&queue_);
}

void AckTracker::Track(const ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Entry> entry(new Entry(
      create_backoff_entry_callback_.Run(&kDefaultBackoffPolicy), ids));
  // This is a small hack. When net::BackoffRequest is first created,
  // GetReleaseTime() always returns the default base::TimeTicks value: 0.
  // In order to work around that, we mark it as failed right away.
  entry->backoff->InformOfRequest(false /* succeeded */);
  const base::TimeTicks release_time = entry->backoff->GetReleaseTime();
  queue_.insert(std::make_pair(release_time, entry.release()));
  NudgeTimer();
}

void AckTracker::Ack(const ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // We could be clever and maintain a mapping of object IDs to their position
  // in the multimap, but that makes things a lot more complicated.
  for (std::multimap<base::TimeTicks, Entry*>::iterator it = queue_.begin();
       it != queue_.end(); ) {
    ObjectIdSet remaining_ids;
    std::set_difference(it->second->ids.begin(), it->second->ids.end(),
                        ids.begin(), ids.end(),
                        std::inserter(remaining_ids, remaining_ids.begin()),
                        ids.value_comp());
    it->second->ids.swap(remaining_ids);
    if (it->second->ids.empty()) {
      std::multimap<base::TimeTicks, Entry*>::iterator erase_it = it;
      ++it;
      delete erase_it->second;
      queue_.erase(erase_it);
    } else {
      ++it;
    }
  }
  NudgeTimer();
}

void AckTracker::NudgeTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (queue_.empty()) {
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  // There are two cases when the timer needs to be started:
  // 1. |desired_run_time_| is in the past. By definition, the timer has already
  //    fired at this point. Since the queue is non-empty, we need to set the
  //    timer to fire again.
  // 2. The timer is already running but we need it to fire sooner if the first
  //    entry's timeout occurs before |desired_run_time_|.
  if (desired_run_time_ <= now || queue_.begin()->first < desired_run_time_) {
    base::TimeDelta delay = queue_.begin()->first - now;
    if (delay < base::TimeDelta()) {
      delay = base::TimeDelta();
    }
    timer_.Start(FROM_HERE, delay, this, &AckTracker::OnTimeout);
    desired_run_time_ = queue_.begin()->first;
  }
}

void AckTracker::OnTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  OnTimeoutAt(tick_clock_->NowTicks());
}

void AckTracker::OnTimeoutAt(base::TimeTicks now) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (queue_.empty())
    return;

  ObjectIdSet expired_ids;
  std::multimap<base::TimeTicks, Entry*>::iterator end =
      queue_.upper_bound(now);
  std::vector<Entry*> expired_entries;
  for (std::multimap<base::TimeTicks, Entry*>::iterator it = queue_.begin();
       it != end; ++it) {
    expired_ids.insert(it->second->ids.begin(), it->second->ids.end());
    it->second->backoff->InformOfRequest(false /* succeeded */);
    expired_entries.push_back(it->second);
  }
  queue_.erase(queue_.begin(), end);
  for (std::vector<Entry*>::const_iterator it = expired_entries.begin();
       it != expired_entries.end(); ++it) {
    queue_.insert(std::make_pair((*it)->backoff->GetReleaseTime(), *it));
  }
  delegate_->OnTimeout(expired_ids);
  NudgeTimer();
}

// Testing helpers.
void AckTracker::SetCreateBackoffEntryCallbackForTest(
    const CreateBackoffEntryCallback& create_backoff_entry_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  create_backoff_entry_callback_ = create_backoff_entry_callback;
}

bool AckTracker::TriggerTimeoutAtForTest(base::TimeTicks now) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool no_timeouts_before_now = (queue_.lower_bound(now) == queue_.begin());
  OnTimeoutAt(now);
  return no_timeouts_before_now;
}

bool AckTracker::IsQueueEmptyForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return queue_.empty();
}

const base::Timer& AckTracker::GetTimerForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return timer_;
}

}  // namespace syncer
