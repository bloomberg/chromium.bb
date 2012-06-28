// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An UpdateApplicator is used to iterate over a number of unapplied updates,
// applying them to the client using the given syncer session.
//
// UpdateApplicator might resemble an iterator, but it actually keeps retrying
// failed updates until no remaining updates can be successfully applied.

#ifndef SYNC_ENGINE_UPDATE_APPLICATOR_H_
#define SYNC_ENGINE_UPDATE_APPLICATOR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/port.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/syncable/syncable_id.h"

namespace syncable {
class WriteTransaction;
class Entry;
}

namespace syncer {

namespace sessions {
class ConflictProgress;
class UpdateProgress;
}

class ConflictResolver;
class Cryptographer;

class UpdateApplicator {
 public:
  typedef std::vector<int64>::iterator UpdateIterator;

  UpdateApplicator(ConflictResolver* resolver,
                   Cryptographer* cryptographer,
                   const UpdateIterator& begin,
                   const UpdateIterator& end,
                   const ModelSafeRoutingInfo& routes,
                   ModelSafeGroup group_filter);
  ~UpdateApplicator();

  // returns true if there's more we can do.
  bool AttemptOneApplication(syncable::WriteTransaction* trans);
  // return true if we've applied all updates.
  bool AllUpdatesApplied() const;

  // This class does not automatically save its progress into the
  // SyncSession -- to get that to happen, call this method after update
  // application is finished (i.e., when AttemptOneAllocation stops returning
  // true).
  void SaveProgressIntoSessionState(
      sessions::ConflictProgress* conflict_progress,
      sessions::UpdateProgress* update_progress);

 private:
  // Track the status of all applications.
  class ResultTracker {
   public:
     explicit ResultTracker(size_t num_results);
     virtual ~ResultTracker();
     void AddSimpleConflict(syncable::Id);
     void AddEncryptionConflict(syncable::Id);
     void AddHierarchyConflict(syncable::Id);
     void AddSuccess(syncable::Id);
     void SaveProgress(sessions::ConflictProgress* conflict_progress,
                       sessions::UpdateProgress* update_progress);
     void ClearConflicts();

     // Returns true iff conflicting_ids_ is empty. Does not check
     // encryption_conflict_ids_.
     bool no_conflicts() const;
   private:
    std::vector<syncable::Id> conflicting_ids_;
    std::vector<syncable::Id> successful_ids_;
    std::vector<syncable::Id> encryption_conflict_ids_;
    std::vector<syncable::Id> hierarchy_conflict_ids_;
  };

  // If true, AttemptOneApplication will skip over |entry| and return true.
  bool SkipUpdate(const syncable::Entry& entry);

  // Adjusts the UpdateIterator members to move ahead by one update.
  void Advance();

  // Used to resolve conflicts when trying to apply updates.
  ConflictResolver* const resolver_;

  // Used to decrypt sensitive sync nodes.
  Cryptographer* cryptographer_;

  UpdateIterator const begin_;
  UpdateIterator end_;
  UpdateIterator pointer_;
  ModelSafeGroup group_filter_;
  bool progress_;

  const ModelSafeRoutingInfo routing_info_;

  // Track the result of the attempts to update applications.
  ResultTracker application_results_;

  DISALLOW_COPY_AND_ASSIGN(UpdateApplicator);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_UPDATE_APPLICATOR_H_
