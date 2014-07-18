// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to track the outstanding work required to bring the client back into
// sync with the server.
#ifndef SYNC_SESSIONS_NUDGE_TRACKER_H_
#define SYNC_SESSIONS_NUDGE_TRACKER_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation_interface.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/data_type_tracker.h"

namespace syncer {

class ObjectIdInvalidationMap;

namespace sessions {

class SYNC_EXPORT_PRIVATE NudgeTracker {
 public:
  static size_t kDefaultMaxPayloadsPerType;

  NudgeTracker();
  ~NudgeTracker();

  // Returns true if there is a good reason for performing a sync cycle.
  // This does not take into account whether or not this is a good *time* to
  // perform a sync cycle; that's the scheduler's job.
  bool IsSyncRequired() const;

  // Returns true if there is a good reason for performing a get updates
  // request as part of the next sync cycle.
  bool IsGetUpdatesRequired() const;

  // Return true if should perform a sync cycle for GU retry.
  //
  // This is sensitive to changes in 'current time'.  Its value can be affected
  // by SetSyncCycleStartTime(), SetNextRetryTime(), and
  // RecordSuccessfulSyncCycle().  Please refer to those functions for more
  // information on how this flag is maintained.
  bool IsRetryRequired() const;

  // Tells this class that all required update fetching or committing has
  // completed successfully.
  void RecordSuccessfulSyncCycle();

  // Takes note of a local change.
  void RecordLocalChange(ModelTypeSet types);

  // Takes note of a locally issued request to refresh a data type.
  void RecordLocalRefreshRequest(ModelTypeSet types);

  // Takes note of the receipt of an invalidation notice from the server.
  void RecordRemoteInvalidation(syncer::ModelType type,
                                scoped_ptr<InvalidationInterface> invalidation);

  // Take note that an initial sync is pending for this type.
  void RecordInitialSyncRequired(syncer::ModelType type);

  // These functions should be called to keep this class informed of the status
  // of the connection to the invalidations server.
  void OnInvalidationsEnabled();
  void OnInvalidationsDisabled();

  // Marks |types| as being throttled from |now| until |now| + |length|.
  void SetTypesThrottledUntil(ModelTypeSet types,
                              base::TimeDelta length,
                              base::TimeTicks now);

  // Removes any throttling that have expired by time |now|.
  void UpdateTypeThrottlingState(base::TimeTicks now);

  // Returns the time of the next type unthrottling, relative to
  // the input |now| value.
  base::TimeDelta GetTimeUntilNextUnthrottle(base::TimeTicks now) const;

  // Returns true if any type is currenlty throttled.
  bool IsAnyTypeThrottled() const;

  // Returns true if |type| is currently throttled.
  bool IsTypeThrottled(ModelType type) const;

  // Returns the set of currently throttled types.
  ModelTypeSet GetThrottledTypes() const;

  // Returns the set of types with local changes pending.
  ModelTypeSet GetNudgedTypes() const;

  // Returns the set of types that have pending invalidations.
  ModelTypeSet GetNotifiedTypes() const;

  // Returns the set of types that have pending refresh requests.
  ModelTypeSet GetRefreshRequestedTypes() const;

  // Returns the 'source' of the GetUpdate request.
  //
  // This flag is deprecated, but still used by the server.  There can be more
  // than one reason to perform a particular sync cycle.  The GetUpdatesTrigger
  // message will contain more reliable information about the reasons for
  // performing a sync.
  //
  // See the implementation for important information about the coalesce logic.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource GetLegacySource() const;

  // Fills a GetUpdatesTrigger message for the next GetUpdates request.  This is
  // used by the DownloadUpdatesCommand to dump lots of useful per-type state
  // information into the GetUpdate request before sending it off to the server.
  void FillProtoMessage(
      ModelType type,
      sync_pb::GetUpdateTriggers* msg) const;

  // Fills a ProgressMarker with single legacy notification hint expected by the
  // sync server.  Newer servers will rely on the data set by FillProtoMessage()
  // instead of this.
  void SetLegacyNotificationHint(
      ModelType type,
      sync_pb::DataTypeProgressMarker* progress) const;

  // Flips the flag if we're due for a retry.
  void SetSyncCycleStartTime(base::TimeTicks now);

  // Adjusts the number of hints that can be stored locally.
  void SetHintBufferSize(size_t size);

  // Schedules a retry GetUpdate request for some time in the future.
  //
  // This is a request sent to us as part of a server response requesting
  // that the client perform a GetUpdate request at |next_retry_time| to
  // fetch any updates it may have missed in the first attempt.
  //
  // To avoid strange results from IsRetryRequired() during a sync cycle, the
  // effects of this change are not guaranteed to take effect until
  // SetSyncCycleStartTime() is called at the start of the *next* sync cycle.
  void SetNextRetryTime(base::TimeTicks next_retry_time);

 private:
  typedef std::map<ModelType, DataTypeTracker*> TypeTrackerMap;

  TypeTrackerMap type_trackers_;
  STLValueDeleter<TypeTrackerMap> type_tracker_deleter_;

  // Merged updates source.  This should be obsolete, but the server still
  // relies on it for some heuristics.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source_;

  // Tracks whether or not invalidations are currently enabled.
  bool invalidations_enabled_;

  // This flag is set if suspect that some technical malfunction or known bug
  // may have left us with some unserviced invalidations.
  //
  // Keeps track of whether or not we're fully in sync with the invalidation
  // server.  This can be false even if invalidations are enabled and working
  // correctly.  For example, until we get ack-tracking working properly, we
  // won't persist invalidations between restarts, so we may be out of sync when
  // we restart.  The only way to get back into sync is to have invalidations
  // enabled, then complete a sync cycle to make sure we're fully up to date.
  bool invalidations_out_of_sync_;

  size_t num_payloads_per_type_;

  base::TimeTicks last_successful_sync_time_;

  // A pending update to the current_retry_time_.
  //
  // The GU retry time is specified by a call to SetNextRetryTime, but we don't
  // want that change to take effect right away, since it could happen in the
  // middle of a sync cycle.  We delay the update until the start of the next
  // sync cycle, which is indicated by a call to SetSyncCycleStartTime().
  base::TimeTicks next_retry_time_;

  // The currently active retry GU time.  Will be null if there is no retry GU
  // pending at this time.
  base::TimeTicks current_retry_time_;

  // The time when the sync cycle started.  This value is maintained by
  // SetSyncCycleStartTime().  This may contain a stale value if we're not
  // currently in a sync cycle.
  base::TimeTicks sync_cycle_start_time_;

  DISALLOW_COPY_AND_ASSIGN(NudgeTracker);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_NUDGE_TRACKER_H_
