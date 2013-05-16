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
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {
namespace sessions {

struct SyncSourceInfo;

class SYNC_EXPORT_PRIVATE NudgeTracker {
 public:
  static size_t kDefaultMaxPayloadsPerType;

  NudgeTracker();
  ~NudgeTracker();

  // Returns true if one of the main reasons for performing the sync cycle is to
  // fetch updates.  This is true when we have pending invalidations or refresh
  // requests.
  bool IsGetUpdatesRequired();

  // Returns true if there is a good reason for performing a sync cycle.
  // This does not take into account whether or not this is a good *time* to
  // perform a sync cycle; that's the scheduler's job.
  bool IsSyncRequired();

  // Tells this class that all required update fetching and committing has
  // completed successfully.
  void RecordSuccessfulSyncCycle();

  // Takes note of a local change.
  void RecordLocalChange(ModelTypeSet types);

  // Takes note of a locally issued request to refresh a data type.
  void RecordLocalRefreshRequest(ModelTypeSet types);

  // Takes note of the receipt of an invalidation notice from the server.
  void RecordRemoteInvalidation(
      const ModelTypeInvalidationMap& invalidation_map);

  // These functions should be called to keep this class informed of the status
  // of the connection to the invalidations server.
  void OnInvalidationsEnabled();
  void OnInvalidationsDisabled();

  // A helper to return an old-style source info.  Used only to maintain
  // compatibility with some old code.
  SyncSourceInfo GetSourceInfo() const;

  // Returns the set of locally modified types, according to the nudges received
  // since the last successful sync cycle.
  ModelTypeSet GetLocallyModifiedTypes() const;

  // Returns the 'source' of the GetUpdate request.
  //
  // This flag is deprecated, but still used by the server.  There can be more
  // than one reason to perform a particular sync cycle.  The GetUpdatesTrigger
  // message will contain more reliable information about the reasons for
  // performing a sync.
  //
  // See the implementation for important information about the coalesce logic.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source() const;

  // Fills a ProgressMarker for the next GetUpdates request.  This is used by
  // the DownloadUpdatesCommand to dump lots of useful per-type state
  // information into the GetUpdate request before sending it off to the server.
  void FillProtoMessage(
      ModelType type,
      sync_pb::GetUpdateTriggers* msg) const;

  // Adjusts the number of hints that can be stored locally.
  void SetHintBufferSize(size_t size);

 private:
  typedef std::list<std::string> PayloadList;
  typedef std::map<ModelType, PayloadList> PayloadListMap;
  typedef std::map<ModelType, int> NudgeMap;

  // Merged updates source.  This should be obsolete, but the server still
  // relies on it for some heuristics.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source_;

  // The number of times each type has been locally nudged since the last
  // successful sync cycle.  If a type is not in the map, the count is zero.
  NudgeMap local_nudge_counts_;

  // The number of times a refresh was requested each type, since the last
  // successful sync cycle.  If a type is not in the map, the count is zero.
  NudgeMap refresh_requested_counts_;

  // A map of datatypes to lists of hints.  The hints are ordered from least
  // recent to most recent.
  PayloadListMap payload_list_map_;

  // Tracks the types for which the list of pending hints has overflowed,
  // causing us to drop the oldest hints.
  ModelTypeSet locally_dropped_payload_types_;

  // Tracks the types for which the invalidation server has notified us that it
  // dropped some of its payloads.
  ModelTypeSet server_dropped_payload_types_;

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

  DISALLOW_COPY_AND_ASSIGN(NudgeTracker);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_NUDGE_TRACKER_H_
