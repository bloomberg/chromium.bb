// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusController handles all counter and status related number crunching and
// state tracking on behalf of a SyncSession.  It 'controls' the model data
// defined in session_state.h.  The most important feature of StatusController
// is the ScopedModelSafetyRestriction. When one of these is active, the
// underlying data set exposed via accessors is swapped out to the appropriate
// set for the restricted ModelSafeGroup behind the scenes.  For example, if
// GROUP_UI is set, then accessors such as conflict_progress() and commit_ids()
// are implicitly restricted to returning only data pertaining to GROUP_UI.
// You can see which parts of status fall into this "restricted" category, or
// the global "shared" category for all model types, by looking at the struct
// declarations in session_state.h. If these accessors are invoked without a
// restriction in place, this is a violation and will cause debug assertions
// to surface improper use of the API in development.  Likewise for
// invocation of "shared" accessors when a restriction is in place; for
// safety's sake, an assertion will fire.
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
#pragma once

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "sync/sessions/ordered_commit_set.h"
#include "sync/sessions/session_state.h"

namespace browser_sync {
namespace sessions {

class StatusController {
 public:
  explicit StatusController(const ModelSafeRoutingInfo& routes);
  ~StatusController();

  // Returns true if some portion of the session state has changed (is dirty)
  // since it was created or was last reset.
  bool TestAndClearIsDirty();

  // Progress counters.  All const methods may return NULL if the
  // progress structure doesn't exist, but all non-const methods
  // auto-create.
  const ConflictProgress* conflict_progress() const;
  ConflictProgress* mutable_conflict_progress();
  const UpdateProgress* update_progress() const;
  UpdateProgress* mutable_update_progress();
  const ConflictProgress* GetUnrestrictedConflictProgress(
      ModelSafeGroup group) const;
  ConflictProgress* GetUnrestrictedMutableConflictProgressForTest(
      ModelSafeGroup group);
  const UpdateProgress* GetUnrestrictedUpdateProgress(
      ModelSafeGroup group) const;
  UpdateProgress* GetUnrestrictedMutableUpdateProgressForTest(
      ModelSafeGroup group);

  // ClientToServer messages.
  const syncable::ModelTypeSet updates_request_types() const {
    return shared_.updates_request_types;
  }
  void set_updates_request_types(syncable::ModelTypeSet value) {
    shared_.updates_request_types = value;
  }
  const ClientToServerResponse& updates_response() const {
    return shared_.updates_response;
  }
  ClientToServerResponse* mutable_updates_response() {
    return &shared_.updates_response;
  }

  // Errors and SyncerStatus.
  const ErrorCounters& error() const {
    return shared_.error.value();
  }
  const SyncerStatus& syncer_status() const {
    return shared_.syncer_status.value();
  }

  // Changelog related state.
  int64 num_server_changes_remaining() const {
    return shared_.num_server_changes_remaining.value();
  }

  const OrderedCommitSet::Projection& commit_id_projection(
      const sessions::OrderedCommitSet &commit_set) {
    DCHECK(group_restriction_in_effect_)
        << "No group restriction for projection.";
    return commit_set.GetCommitIdProjection(group_restriction_);
  }

  // Control parameters for sync cycles.
  bool conflicts_resolved() const {
    return shared_.control_params.conflicts_resolved;
  }

  // If a GetUpdates for any data type resulted in downloading an update that
  // is in conflict, this method returns true.
  // Note: this includes unresolvable conflicts.
  bool HasConflictingUpdates() const;

  // Aggregate sums of various types of conflict counters accross all
  // ConflictProgress objects (one for each ModelSafeGroup currently in-use).
  int TotalNumEncryptionConflictingItems() const;
  int TotalNumHierarchyConflictingItems() const;
  int TotalNumServerConflictingItems() const;
  int TotalNumSimpleConflictingItems() const;

  // Aggregate sum of SimpleConflictingItemSize() and other
  // ${Type}ConflictingItemSize() methods over all ConflictProgress objects (one
  // for each ModelSafeGroup currently in-use).
  int TotalNumConflictingItems() const;

  // Returns the number of updates received from the sync server.
  int64 CountUpdates() const;

  // Returns true if the last download_updates_command received a valid
  // server response.
  bool download_updates_succeeded() const {
    return shared_.error.value().last_download_updates_result
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
    return ActiveGroupRestrictionIncludesModel(syncable::BOOKMARKS);
  }

  // A toolbelt full of methods for updating counters and flags.
  void set_num_server_changes_remaining(int64 changes_remaining);
  void set_invalid_store(bool invalid_store);
  void set_num_successful_bookmark_commits(int value);
  void increment_num_successful_commits();
  void increment_num_successful_bookmark_commits();
  void increment_num_updates_downloaded_by(int value);
  void increment_num_tombstone_updates_downloaded_by(int value);
  void increment_num_reflected_updates_downloaded_by(int value);
  void set_types_needing_local_migration(syncable::ModelTypeSet types);
  void increment_num_local_overwrites();
  void increment_num_server_overwrites();
  void set_sync_protocol_error(const SyncProtocolError& error);
  void set_last_download_updates_result(const SyncerError result);
  void set_commit_result(const SyncerError result);

  void update_conflicts_resolved(bool resolved);
  void reset_conflicts_resolved();

  void UpdateStartTime();

  void set_debug_info_sent();

  bool debug_info_sent() const;

 private:
  friend class ScopedModelSafeGroupRestriction;

  // Check whether a particular model is included by the active group
  // restriction.
  bool ActiveGroupRestrictionIncludesModel(syncable::ModelType model) const {
    if (!group_restriction_in_effect_)
      return true;
    ModelSafeRoutingInfo::const_iterator it = routing_info_.find(model);
    if (it == routing_info_.end())
      return false;
    return group_restriction() == it->second;
  }

  // Returns the state, if it exists, or NULL otherwise.
  const PerModelSafeGroupState* GetModelSafeGroupState(
      bool restrict, ModelSafeGroup group) const;

  // Helper to lazily create objects for per-ModelSafeGroup state.
  PerModelSafeGroupState* GetOrCreateModelSafeGroupState(
      bool restrict, ModelSafeGroup group);

  AllModelTypeState shared_;
  std::map<ModelSafeGroup, PerModelSafeGroupState*> per_model_group_;

  STLValueDeleter<std::map<ModelSafeGroup, PerModelSafeGroupState*> >
      per_model_group_deleter_;

  // Set to true if any DirtyOnWrite pieces of state we maintain are changed.
  // Reset to false by TestAndClearIsDirty.
  bool is_dirty_;

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

}
}

#endif  // SYNC_SESSIONS_STATUS_CONTROLLER_H_
