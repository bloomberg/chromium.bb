// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_DATA_TYPE_TRACKER_H_
#define SYNC_SESSIONS_DATA_TYPE_TRACKER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/invalidation_interface.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

class InvalidationInterface;

namespace sessions {

// A class to track the per-type scheduling data.
class DataTypeTracker {
 public:
  explicit DataTypeTracker();
  ~DataTypeTracker();

  // For STL compatibility, we do not forbid the creation of a default copy
  // constructor and assignment operator.

  // Tracks that a local change has been made to this type.
  // Returns the current local change nudge delay for this type.
  base::TimeDelta RecordLocalChange();

  // Tracks that a local refresh request has been made for this type.
  void RecordLocalRefreshRequest();

  // Tracks that we received invalidation notifications for this type.
  void RecordRemoteInvalidation(scoped_ptr<InvalidationInterface> incoming);

  // Takes note that initial sync is pending for this type.
  void RecordInitialSyncRequired();

  // Records that a sync cycle has been performed successfully.
  // Generally, this means that all local changes have been committed and all
  // remote changes have been downloaded, so we can clear any flags related to
  // pending work.
  void RecordSuccessfulSyncCycle();

  // Updates the size of the invalidations payload buffer.
  void UpdatePayloadBufferSize(size_t new_size);

  // Returns true if there is a good reason to perform a sync cycle.  This does
  // not take into account whether or not now is a good time to perform a sync
  // cycle.  That's for the scheduler to decide.
  bool IsSyncRequired() const;

  // Returns true if there is a good reason to fetch updates for this type as
  // part of the next sync cycle.
  bool IsGetUpdatesRequired() const;

  // Returns true if there is an uncommitted local change.
  bool HasLocalChangePending() const;

  // Returns true if we've received an invalidation since we last fetched
  // updates.
  bool HasPendingInvalidation() const;

  // Returns true if an explicit refresh request is still outstanding.
  bool HasRefreshRequestPending() const;

  // Returns true if this type is requesting an initial sync.
  bool IsInitialSyncRequired() const;

  // Fills in the legacy invalidaiton payload information fields.
  void SetLegacyNotificationHint(
      sync_pb::DataTypeProgressMarker* progress) const;

  // Fills some type-specific contents of a GetUpdates request protobuf.  These
  // messages provide the server with the information it needs to decide how to
  // handle a request.
  void FillGetUpdatesTriggersMessage(sync_pb::GetUpdateTriggers* msg) const;

  // Returns true if the type is currently throttled.
  bool IsThrottled() const;

  // Returns the time until this type's throttling interval expires.  Should not
  // be called unless IsThrottled() returns true.  The returned value will be
  // increased to zero if it would otherwise have been negative.
  base::TimeDelta GetTimeUntilUnthrottle(base::TimeTicks now) const;

  // Throttles the type from |now| until |now| + |duration|.
  void ThrottleType(base::TimeDelta duration, base::TimeTicks now);

  // Unthrottles the type if |now| >= the throttle expiry time.
  void UpdateThrottleState(base::TimeTicks now);

  // Update the local change nudge delay for this type.
  void UpdateLocalNudgeDelay(base::TimeDelta delay);

 private:
  // Number of local change nudges received for this type since the last
  // successful sync cycle.
  int local_nudge_count_;

  // Number of local refresh requests received for this type since the last
  // successful sync cycle.
  int local_refresh_request_count_;

  // The list of invalidations received since the last successful sync cycle.
  // This list may be incomplete.  See also:
  // drop_tracker_.IsRecoveringFromDropEvent() and server_payload_overflow_.
  //
  // This list takes ownership of its contents.
  ScopedVector<InvalidationInterface> pending_invalidations_;

  size_t payload_buffer_size_;

  // Set to true if this type is ready for, but has not yet completed initial
  // sync.
  bool initial_sync_required_;

  // If !unthrottle_time_.is_null(), this type is throttled and may not download
  // or commit data until the specified time.
  base::TimeTicks unthrottle_time_;

  // A helper to keep track invalidations we dropped due to overflow.
  scoped_ptr<InvalidationInterface> last_dropped_invalidation_;

  // The amount of time to delay a sync cycle by when a local change for this
  // type occurs.
  base::TimeDelta nudge_delay_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeTracker);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_DATA_TYPE_TRACKER_H_
