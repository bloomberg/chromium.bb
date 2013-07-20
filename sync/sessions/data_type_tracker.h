// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to track the per-type scheduling data.
#ifndef SYNC_SESSIONS_DATA_TYPE_TRACKER_H_
#define SYNC_SESSIONS_DATA_TYPE_TRACKER_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {
namespace sessions {

typedef std::deque<std::string> PayloadList;

class DataTypeTracker {
 public:
  DataTypeTracker();
  ~DataTypeTracker();

  // For STL compatibility, we do not forbid the creation of a default copy
  // constructor and assignment operator.

  // Tracks that a local change has been made to this type.
  void RecordLocalChange();

  // Tracks that a local refresh request has been made for this type.
  void RecordLocalRefreshRequest();

  // Tracks that we received an invalidation notification for this type.
  void RecordRemoteInvalidation(const std::string& payload);

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

  // Returns the most recent invalidation payload.
  std::string GetMostRecentInvalidationPayload() const;

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

 private:
  // Number of local change nudges received for this type since the last
  // successful sync cycle.
  int local_nudge_count_;

  // Number of local refresh requests received for this type since the last
  // successful sync cycle.
  int local_refresh_request_count_;

  // The list of invalidation payloads received since the last successful sync
  // cycle.  This list may be incomplete.  See also: local_payload_overflow_ and
  // server_payload_overflow_.
  PayloadList pending_payloads_;

  // This flag is set if the the local buffer space was been exhausted, causing
  // us to prematurely discard the invalidation payloads stored locally.
  bool local_payload_overflow_;

  // This flag is set if the server buffer space was exchauseted, causing the
  // server to prematurely discard some invalidation payloads.
  bool server_payload_overflow_;

  size_t payload_buffer_size_;

  // If !unthrottle_time_.is_null(), this type is throttled and may not download
  // or commit data until the specified time.
  base::TimeTicks unthrottle_time_;
};

}  // namespace syncer
}  // namespace sessions

#endif  // SYNC_SESSIONS_DATA_TYPE_TRACKER_H_
