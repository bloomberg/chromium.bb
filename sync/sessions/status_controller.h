// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusController handles all counter and status related number crunching and
// state tracking on behalf of a SyncSession.
//
// The most important feature of StatusController is the
// ScopedModelSafeGroupRestriction.  Some of its functions expose per-thread
// state, and can be called only when the restriction is in effect.  For
// example, if GROUP_UI is set then the value returned from
// commit_id_projection() will be useful for iterating over the commit IDs of
// items that live on the UI thread.
//
// Other parts of its state are global, and do not require the restriction.
//
// NOTE: There is no concurrent access protection provided by this class. It
// assumes one single thread is accessing this class for each unique
// ModelSafeGroup, and also only one single thread (in practice, the
// SyncerThread) responsible for all "shared" access when no restriction is in
// place. Thus, every bit of data is to be accessed mutually exclusively with
// respect to threads.
//
// StatusController can also track if changes occur to certain parts of state
// so that various parts of the sync engine can avoid broadcasting
// notifications if no changes occurred.

#ifndef SYNC_SESSIONS_STATUS_CONTROLLER_H_
#define SYNC_SESSIONS_STATUS_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"
#include "sync/sessions/ordered_commit_set.h"

namespace syncer {
namespace sessions {

class SYNC_EXPORT_PRIVATE StatusController {
 public:
  explicit StatusController(const ModelSafeRoutingInfo& routes);
  ~StatusController();

  // ClientToServer messages.
  const ModelTypeSet updates_request_types() const {
    return model_neutral_.updates_request_types;
  }
  void set_updates_request_types(ModelTypeSet value) {
    model_neutral_.updates_request_types = value;
  }
  const sync_pb::ClientToServerResponse& updates_response() const {
    return model_neutral_.updates_response;
  }
  sync_pb::ClientToServerResponse* mutable_updates_response() {
    return &model_neutral_.updates_response;
  }

  // Changelog related state.
  int64 num_server_changes_remaining() const {
    return model_neutral_.num_server_changes_remaining;
  }

  const OrderedCommitSet::Projection& commit_id_projection(
      const sessions::OrderedCommitSet &commit_set) {
    DCHECK(group_restriction_in_effect_)
        << "No group restriction for projection.";
    return commit_set.GetCommitIdProjection(group_restriction_);
  }

  // Various conflict counters.
  int num_encryption_conflicts() const;
  int num_hierarchy_conflicts() const;
  int num_server_conflicts() const;

  // Aggregate sum of all conflicting items over all conflict types.
  int TotalNumConflictingItems() const;

  // Number of successfully applied updates.
  int num_updates_applied() const;

  int num_server_overwrites() const;

  // Returns the number of updates received from the sync server.
  int64 CountUpdates() const;

  // Returns true if the last download_updates_command received a valid
  // server response.
  bool download_updates_succeeded() const {
    return model_neutral_.last_download_updates_result
        == SYNCER_OK;
  }

  // Returns true if the last updates response indicated that we were fully
  // up to date.  This is subtle: if it's false, it could either mean that
  // the server said there WAS more to download, or it could mean that we
  // were unable to reach the server.  If we didn't request every enabled
  // datatype, then we can't say for sure that there's nothing left to
  // download: in that case, this also returns false.
  bool ServerSaysNothingMoreToDownload() const;

  ModelSafeGroup group_restriction() const {
    return group_restriction_;
  }

  base::Time sync_start_time() const {
    // The time at which we sent the first GetUpdates command for this sync.
    return sync_start_time_;
  }

  bool HasBookmarkCommitActivity() const {
    return ActiveGroupRestrictionIncludesModel(BOOKMARKS);
  }

  const ModelNeutralState& model_neutral_state() const {
    return model_neutral_;
  }

  SyncerError last_get_key_result() const;

  // Download counters.
  void set_num_server_changes_remaining(int64 changes_remaining);
  void increment_num_updates_downloaded_by(int value);
  void increment_num_tombstone_updates_downloaded_by(int value);
  void increment_num_reflected_updates_downloaded_by(int value);

  // Update application and conflict resolution counters.
  void increment_num_updates_applied_by(int value);
  void increment_num_encryption_conflicts_by(int value);
  void increment_num_hierarchy_conflicts_by(int value);
  void increment_num_server_conflicts();
  void increment_num_local_overwrites();
  void increment_num_server_overwrites();

  // Commit counters.
  void increment_num_successful_commits();
  void increment_num_successful_bookmark_commits();
  void set_num_successful_bookmark_commits(int value);

  // Server communication status tracking.
  void set_sync_protocol_error(const SyncProtocolError& error);
  void set_last_get_key_result(const SyncerError result);
  void set_last_download_updates_result(const SyncerError result);
  void set_commit_result(const SyncerError result);

  // A very important flag used to inform frontend of need to migrate.
  void set_types_needing_local_migration(ModelTypeSet types);

  void UpdateStartTime();

  void set_debug_info_sent();

  bool debug_info_sent() const;

 private:
  friend class ScopedModelSafeGroupRestriction;

  // Check whether a particular model is included by the active group
  // restriction.
  bool ActiveGroupRestrictionIncludesModel(ModelType model) const {
    if (!group_restriction_in_effect_)
      return true;
    ModelSafeRoutingInfo::const_iterator it = routing_info_.find(model);
    if (it == routing_info_.end())
      return false;
    return group_restriction() == it->second;
  }

  ModelNeutralState model_neutral_;

  // Used to fail read/write operations on state that don't obey the current
  // active ModelSafeWorker contract.
  bool group_restriction_in_effect_;
  ModelSafeGroup group_restriction_;

  const ModelSafeRoutingInfo routing_info_;

  base::Time sync_start_time_;

  DISALLOW_COPY_AND_ASSIGN(StatusController);
};

// A utility to restrict access to only those parts of the given
// StatusController that pertain to the specified ModelSafeGroup.
class ScopedModelSafeGroupRestriction {
 public:
  ScopedModelSafeGroupRestriction(StatusController* to_restrict,
                                  ModelSafeGroup restriction)
      : status_(to_restrict) {
    DCHECK(!status_->group_restriction_in_effect_);
    status_->group_restriction_ = restriction;
    status_->group_restriction_in_effect_ = true;
  }
  ~ScopedModelSafeGroupRestriction() {
    DCHECK(status_->group_restriction_in_effect_);
    status_->group_restriction_in_effect_ = false;
  }
 private:
  StatusController* status_;
  DISALLOW_COPY_AND_ASSIGN(ScopedModelSafeGroupRestriction);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_STATUS_CONTROLLER_H_
