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

  SyncSessionJob(Purpose purpose,
                 base::TimeTicks start,
                 const sessions::SyncSourceInfo& source_info,
                 const ConfigurationParams& config_params);
  ~SyncSessionJob();

  // Returns a new clone of the job, with a cloned SyncSession ready to be
  // retried / rescheduled.
  scoped_ptr<SyncSessionJob> Clone() const;

  // Overwrite the sync update source with the most recent and merge the
  // type/state map.
  void CoalesceSources(const sessions::SyncSourceInfo& source);

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
  bool Finish(bool early_exit, sessions::SyncSession* session);

  static const char* GetPurposeString(Purpose purpose);
  static void GetSyncerStepsForPurpose(Purpose purpose,
                                       SyncerStep* start,
                                       SyncerStep* end);

  Purpose purpose() const;
  const sessions::SyncSourceInfo& source_info() const;
  base::TimeTicks scheduled_start() const;
  void set_scheduled_start(base::TimeTicks start);
  SyncerStep start_step() const;
  SyncerStep end_step() const;
  ConfigurationParams config_params() const;

 private:
  // A SyncSessionJob can be in one of these three states, controlled by the
  // Finish() function, see method comments.
  enum FinishedState {
    NOT_FINISHED,  // Finish has not been called.
    EARLY_EXIT,    // Finish was called but the job was "preempted",
    FINISHED       // Indicates a "clean" finish operation.
  };

  const Purpose purpose_;
  sessions::SyncSourceInfo source_info_;

  base::TimeTicks scheduled_start_;

  // Only used for purpose_ == CONFIGURATION.  This, and different Finish() and
  // Succeeded() behavior may be arguments to subclass in the future.
  const ConfigurationParams config_params_;

  // Set to true if Finish() was called, false otherwise. True implies that
  // a SyncShare operation took place with |session_| and it cycled through
  // all requisite steps given |purpose_| without being preempted.
  FinishedState finished_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionJob);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SESSION_JOB_H_
