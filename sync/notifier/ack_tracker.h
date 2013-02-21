// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_ACK_TRACKER_H_
#define SYNC_NOTIFIER_ACK_TRACKER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/backoff_entry.h"
#include "sync/base/sync_export.h"
#include "sync/notifier/invalidation_util.h"

namespace base {
class TickClock;
}  // namespace base

namespace syncer {

// A simple class that tracks sets of object IDs that have not yet been
// acknowledged. Internally, it manages timeouts for the tracked object IDs and
// periodically triggers a callback for each timeout period. The timeout is a
// simple exponentially increasing time that starts at 60 seconds and is capped
// at 600 seconds.
class SYNC_EXPORT_PRIVATE AckTracker {
 public:
  class SYNC_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate();

    // |ids| contains all object IDs that have timed out in this time interval.
    virtual void OnTimeout(const ObjectIdSet& ids) = 0;
  };

  typedef base::Callback<scoped_ptr<net::BackoffEntry>(
      const net::BackoffEntry::Policy* const)> CreateBackoffEntryCallback;

  AckTracker(base::TickClock* tick_clock, Delegate* delegate);
  ~AckTracker();

  // Equivalent to calling Ack() on all currently registered object IDs.
  void Clear();

  // Starts tracking timeouts for |ids|. Timeouts will be triggered for each
  // object ID until it is acknowledged. Note that no de-duplication is
  // performed; calling Track() twice on the same set of ids will result in two
  // different timeouts being triggered for those ids.
  void Track(const ObjectIdSet& ids);
  // Marks a set of |ids| as acknowledged.
  void Ack(const ObjectIdSet& ids);

  // Testing methods.
  void SetCreateBackoffEntryCallbackForTest(
      const CreateBackoffEntryCallback& create_backoff_entry_callback);
  // Returns true iff there are no timeouts scheduled to occur before |now|.
  // Used in testing to make sure we don't have timeouts set to expire before
  // when they should.
  bool TriggerTimeoutAtForTest(base::TimeTicks now);
  bool IsQueueEmptyForTest() const;
  const base::Timer& GetTimerForTest() const;

 private:
  struct Entry {
    Entry(scoped_ptr<net::BackoffEntry> backoff, const ObjectIdSet& ids);
    ~Entry();

    scoped_ptr<net::BackoffEntry> backoff;
    ObjectIdSet ids;

   private:
    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void NudgeTimer();
  void OnTimeout();
  void OnTimeoutAt(base::TimeTicks now);

  static scoped_ptr<net::BackoffEntry> DefaultCreateBackoffEntryStrategy(
      const net::BackoffEntry::Policy* const policy);

  // Used for testing purposes.
  CreateBackoffEntryCallback create_backoff_entry_callback_;

  base::TickClock* const tick_clock_;

  Delegate* const delegate_;

  base::OneShotTimer<AckTracker> timer_;
  // The time that the timer should fire at. We use this to determine if we need
  // to start or update |timer_| in NudgeTimer(). We can't simply use
  // timer_.desired_run_time() for this purpose because it always uses
  // base::TimeTicks::Now() as a reference point when Timer::Start() is called,
  // while NudgeTimer() needs a fixed reference point to avoid unnecessarily
  // updating the timer.
  base::TimeTicks desired_run_time_;
  std::multimap<base::TimeTicks, Entry*> queue_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AckTracker);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_ACK_TRACKER_H_
