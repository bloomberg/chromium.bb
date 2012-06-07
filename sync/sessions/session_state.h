// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The 'sessions' namespace comprises all the pieces of state that are
// combined to form a SyncSession instance. In that way, it can be thought of
// as an extension of the SyncSession type itself. Session scoping gives
// context to things like "conflict progress", "update progress", etc, and the
// separation this file provides allows clients to only include the parts they
// need rather than the entire session stack.

#ifndef SYNC_SESSIONS_SESSION_STATE_H_
#define SYNC_SESSIONS_SESSION_STATE_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/syncproto.h"
#include "sync/internal_api/public/sessions/error_counters.h"
#include "sync/internal_api/public/sessions/syncer_status.h"
#include "sync/syncable/syncable_id.h"

namespace browser_sync {
namespace sessions {

class UpdateProgress;

// Tracks progress of conflicts and their resolutions.
class ConflictProgress {
 public:
  explicit ConflictProgress(bool* dirty_flag);
  ~ConflictProgress();

  bool HasSimpleConflictItem(const syncable::Id &id) const;

  // Various mutators for tracking commit conflicts.
  void AddSimpleConflictingItemById(const syncable::Id& the_id);
  void EraseSimpleConflictingItemById(const syncable::Id& the_id);
  std::set<syncable::Id>::const_iterator SimpleConflictingItemsBegin() const;
  std::set<syncable::Id>::const_iterator SimpleConflictingItemsEnd() const;
  int SimpleConflictingItemsSize() const {
    return simple_conflicting_item_ids_.size();
  }

  // Mutators for unresolvable conflicting items (see description below).
  void AddEncryptionConflictingItemById(const syncable::Id& the_id);
  int EncryptionConflictingItemsSize() const {
    return num_encryption_conflicting_items;
  }

  void AddHierarchyConflictingItemById(const syncable::Id& id);
  int HierarchyConflictingItemsSize() const {
    return num_hierarchy_conflicting_items;
  }

  void AddServerConflictingItemById(const syncable::Id& id);
  int ServerConflictingItemsSize() const {
    return num_server_conflicting_items;
  }

 private:
  // Conflicts that occur when local and server changes collide and can be
  // resolved locally.
  std::set<syncable::Id> simple_conflicting_item_ids_;

  // Unresolvable conflicts are not processed by the conflict resolver.  We wait
  // and hope the server will provide us with an update that resolves these
  // conflicts.
  std::set<syncable::Id> unresolvable_conflicting_item_ids_;

  size_t num_server_conflicting_items;
  size_t num_hierarchy_conflicting_items;
  size_t num_encryption_conflicting_items;

  // Whether a conflicting item was added or removed since
  // the last call to reset_progress_changed(), if any. In practice this
  // points to StatusController::is_dirty_.
  bool* dirty_;
};

typedef std::pair<VerifyResult, sync_pb::SyncEntity> VerifiedUpdate;
typedef std::pair<UpdateAttemptResponse, syncable::Id> AppliedUpdate;

// Tracks update application and verification.
class UpdateProgress {
 public:
  UpdateProgress();
  ~UpdateProgress();

  void AddVerifyResult(const VerifyResult& verify_result,
                       const sync_pb::SyncEntity& entity);

  // Log a successful or failing update attempt.
  void AddAppliedUpdate(const UpdateAttemptResponse& response,
                        const syncable::Id& id);

  // Various iterators.
  std::vector<AppliedUpdate>::iterator AppliedUpdatesBegin();
  std::vector<VerifiedUpdate>::const_iterator VerifiedUpdatesBegin() const;
  std::vector<AppliedUpdate>::const_iterator AppliedUpdatesEnd() const;
  std::vector<VerifiedUpdate>::const_iterator VerifiedUpdatesEnd() const;

  // Returns the number of update application attempts.  This includes both
  // failures and successes.
  int AppliedUpdatesSize() const { return applied_updates_.size(); }
  int VerifiedUpdatesSize() const { return verified_updates_.size(); }
  bool HasVerifiedUpdates() const { return !verified_updates_.empty(); }
  bool HasAppliedUpdates() const { return !applied_updates_.empty(); }
  void ClearVerifiedUpdates() { verified_updates_.clear(); }

  // Count the number of successful update applications that have happend this
  // cycle. Note that if an item is successfully applied twice, it will be
  // double counted here.
  int SuccessfullyAppliedUpdateCount() const;

  // Returns true if at least one update application failed due to a conflict
  // during this sync cycle.
  bool HasConflictingUpdates() const;

 private:
  // Container for updates that passed verification.
  std::vector<VerifiedUpdate> verified_updates_;

  // Stores the result of the various ApplyUpdate attempts we've made.
  // May contain duplicate entries.
  std::vector<AppliedUpdate> applied_updates_;
};

struct SyncCycleControlParameters {
  SyncCycleControlParameters() : conflicts_resolved(false),
                                 items_committed(false),
                                 debug_info_sent(false) {}
  // Set to true by ResolveConflictsCommand if any forward progress was made.
  bool conflicts_resolved;

  // Set to true by PostCommitMessageCommand if any commits were successful.
  bool items_committed;

  // True indicates debug info has been sent once this session.
  bool debug_info_sent;
};

// DirtyOnWrite wraps a value such that any write operation will update a
// specified dirty bit, which can be used to determine if a notification should
// be sent due to state change.
template <typename T>
class DirtyOnWrite {
 public:
  explicit DirtyOnWrite(bool* dirty) : dirty_(dirty) {}
  DirtyOnWrite(bool* dirty, const T& t) : t_(t), dirty_(dirty) {}
  T* mutate() {
    *dirty_ = true;
    return &t_;
  }
  const T& value() const { return t_; }
 private:
  T t_;
  bool* dirty_;
};

// The next 3 structures declare how all the state involved in running a sync
// cycle is divided between global scope (applies to all model types),
// ModelSafeGroup scope (applies to all data types in a group), and single
// model type scope.  Within this breakdown, each struct declares which bits
// of state are dirty-on-write and should incur dirty bit updates if changed.

// Grouping of all state that applies to all model types.  Note that some
// components of the global grouping can internally implement finer grained
// scope control (such as OrderedCommitSet), but the top level entity is still
// a singleton with respect to model types.
struct AllModelTypeState {
  explicit AllModelTypeState(bool* dirty_flag);
  ~AllModelTypeState();

  // We GetUpdates for some combination of types at once.
  // requested_update_types stores the set of types which were requested.
  syncable::ModelTypeSet updates_request_types;
  ClientToServerResponse updates_response;
  // Used to build the shared commit message.
  DirtyOnWrite<SyncerStatus> syncer_status;
  DirtyOnWrite<ErrorCounters> error;
  SyncCycleControlParameters control_params;
  DirtyOnWrite<int64> num_server_changes_remaining;
};

// Grouping of all state that applies to a single ModelSafeGroup.
struct PerModelSafeGroupState {
  explicit PerModelSafeGroupState(bool* dirty_flag);
  ~PerModelSafeGroupState();

  UpdateProgress update_progress;
  ConflictProgress conflict_progress;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // SYNC_SESSIONS_SESSION_STATE_H_
