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

#include "sync/engine/syncer_types.h"
#include "sync/engine/syncproto.h"
#include "sync/syncable/syncable_id.h"

namespace syncer {
namespace sessions {

// Tracks progress of conflicts and their resolutions.
class ConflictProgress {
 public:
  explicit ConflictProgress();
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

// Grouping of all state that applies to a single ModelSafeGroup.
struct PerModelSafeGroupState {
  explicit PerModelSafeGroupState();
  ~PerModelSafeGroupState();

  UpdateProgress update_progress;
  ConflictProgress conflict_progress;
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SESSION_STATE_H_
