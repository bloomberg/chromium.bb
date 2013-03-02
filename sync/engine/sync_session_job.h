// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_SESSION_JOB_H_
#define SYNC_ENGINE_SYNC_SESSION_JOB_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "sync/base/sync_export.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer.h"
#include "sync/sessions/sync_session.h"

namespace syncer {

class SYNC_EXPORT_PRIVATE SyncSessionJob {
 public:
  enum Purpose {
    // Uninitialized state, should never be hit in practice.
    UNKNOWN = -1,
    // Our poll timer schedules POLL jobs periodically based on a server
    // assigned poll interval.
    POLL,
    // A nudge task can come from a variety of components needing to force
    // a sync.  The source is inferable from |session.source()|.
    NUDGE,
    // Typically used for fetching updates for a subset of the enabled types
    // during initial sync or reconfiguration.
    CONFIGURATION,
  };

  // This class exists to inform an interested party about the destruction of
  // a SyncSessionJob.
  class SYNC_EXPORT_PRIVATE DestructionObserver {
   public:
    virtual void OnJobDestroyed(SyncSessionJob* job) = 0;
  };

  SyncSessionJob(Purpose purpose,
                 base::TimeTicks start,
                 scoped_ptr<sessions::SyncSession> session,
                 const ConfigurationParams& config_params);
  ~SyncSessionJob();

  // Returns a new clone of the job, with a cloned SyncSession ready to be
  // retried / rescheduled.  The returned job will *never* be a canary,
  // regardless of |this|. A job can only be cloned once it has finished,
  // to prevent bugs where multiple jobs are scheduled with the same session.
  // Use CloneAndAbandon if you want to clone before finishing.
  scoped_ptr<SyncSessionJob> Clone() const;

  // Same as Clone() above, but also ejects the SyncSession from this job,
  // preventing it from ever being used for a sync cycle.
  scoped_ptr<SyncSessionJob> CloneAndAbandon();

  // Record that the scheduler has deemed the job as finished and give it a
  // chance to perform any remaining cleanup and/or notification completion
  // callback invocations.
  // |early_exit| specifies whether the job 1) cycled through all the
  // SyncerSteps it needed, or 2) was pre-empted by the scheduler.
  // Returns true if we completely ran the session without errors.
  // There are many errors that could prevent a sync cycle from succeeding,
  // such as invalid local state, inability to contact the server, inability
  // to authenticate with the server, and server errors.  What they have in
  // common is that the we either need to take some action and then retry the
  // sync cycle or, in the case of transient errors, retry after some backoff
  // timer has expired.  Most importantly, the SyncScheduler should not assume
  // that the original action that triggered the sync cycle (ie. a nudge or a
  // notification) has been properly serviced.
  bool Finish(bool early_exit);

  // Causes is_canary() to return true. Use with caution.
  void GrantCanaryPrivilege();

  static const char* GetPurposeString(Purpose purpose);
  static void GetSyncerStepsForPurpose(Purpose purpose,
                                       SyncerStep* start,
                                       SyncerStep* end);

  bool is_canary() const;
  Purpose purpose() const;
  base::TimeTicks scheduled_start() const;
  void set_scheduled_start(base::TimeTicks start);
  const sessions::SyncSession* session() const;
  sessions::SyncSession* mutable_session();
  SyncerStep start_step() const;
  SyncerStep end_step() const;
  ConfigurationParams config_params() const;

  void set_destruction_observer(
      const base::WeakPtr<DestructionObserver>& observer);
 private:
  // A SyncSessionJob can be in one of these three states, controlled by the
  // Finish() function, see method comments.
  enum FinishedState {
    NOT_FINISHED,  // Finish has not been called.
    EARLY_EXIT,    // Finish was called but the job was "preempted",
    FINISHED       // Indicates a "clean" finish operation.
  };

  scoped_ptr<sessions::SyncSession> CloneSession() const;

  const Purpose purpose_;

  base::TimeTicks scheduled_start_;
  scoped_ptr<sessions::SyncSession> session_;
  bool is_canary_;

  // Only used for purpose_ == CONFIGURATION.  This, and different Finish() and
  // Succeeded() behavior may be arguments to subclass in the future.
  const ConfigurationParams config_params_;

  // Set to true if Finish() was called, false otherwise. True implies that
  // a SyncShare operation took place with |session_| and it cycled through
  // all requisite steps given |purpose_| without being preempted.
  FinishedState finished_;

  base::WeakPtr<DestructionObserver> destruction_observer_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionJob);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SESSION_JOB_H_
