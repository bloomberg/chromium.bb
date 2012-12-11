// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_MODEL_NEUTRAL_STATE_H
#define SYNC_SESSIONS_MODEL_NEUTRAL_STATE_H

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/sync_protocol_error.h"

namespace syncer {
namespace sessions {

// Grouping of all state that applies to all model types.  Note that some
// components of the global grouping can internally implement finer grained
// scope control, but the top level entity is still a singleton with respect to
// model types.
struct SYNC_EXPORT ModelNeutralState {
  ModelNeutralState();
  ~ModelNeutralState();

  // We GetUpdates for some combination of types at once.
  // requested_update_types stores the set of types which were requested.
  ModelTypeSet updates_request_types;

  sync_pb::ClientToServerResponse updates_response;

  int num_successful_commits;

  // This is needed for monitoring extensions activity.
  int num_successful_bookmark_commits;

  // Download event counters.
  int num_updates_downloaded_total;
  int num_tombstone_updates_downloaded_total;
  int num_reflected_updates_downloaded_total;

  // If the syncer encountered a MIGRATION_DONE code, these are the types that
  // the client must now "migrate", by purging and re-downloading all updates.
  ModelTypeSet types_needing_local_migration;

  // Update application and conflicts.
  int num_updates_applied;
  int num_encryption_conflicts;
  int num_server_conflicts;
  int num_hierarchy_conflicts;

  // Overwrites due to conflict resolution counters.
  int num_local_overwrites;
  int num_server_overwrites;

  // Any protocol errors that we received during this sync session.
  SyncProtocolError sync_protocol_error;

  // Records the most recent results of GetKey, PostCommit and GetUpdates
  // commands.
  SyncerError last_get_key_result;
  SyncerError last_download_updates_result;
  SyncerError commit_result;

  // Set to true by PostCommitMessageCommand if any commits were successful.
  bool items_committed;

  // True indicates debug info has been sent once this session.
  bool debug_info_sent;

  // Number of changes remaining, according to the server.
  // Take it as an estimate unless it's value is zero, in which case there
  // really is nothing more to download.
  int64 num_server_changes_remaining;
};

bool HasSyncerError(const ModelNeutralState& state);

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_MODEL_NEUTRAL_STATE_H_
