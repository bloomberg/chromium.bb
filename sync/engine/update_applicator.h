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

#include <vector>

#include "base/basictypes.h"
#include "base/port.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/syncable/syncable_id.h"

namespace syncer {

namespace sessions {
class ConflictProgress;
class UpdateProgress;
}

namespace syncable {
class WriteTransaction;
class Entry;
}

class ConflictResolver;
class Cryptographer;

class UpdateApplicator {
 public:
  typedef std::vector<int64>::iterator UpdateIterator;

  UpdateApplicator(Cryptographer* cryptographer,
                   const ModelSafeRoutingInfo& routes,
                   ModelSafeGroup group_filter);
  ~UpdateApplicator();

  // Attempt to apply the specified updates.
  void AttemptApplications(syncable::WriteTransaction* trans,
                           const std::vector<int64>& handles);

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
     explicit ResultTracker();
     virtual ~ResultTracker();
     void AddSimpleConflict(syncable::Id);
     void AddEncryptionConflict(syncable::Id);
     void AddHierarchyConflict(syncable::Id);
     void AddSuccess(syncable::Id);
     void SaveProgress(sessions::ConflictProgress* conflict_progress,
                       sessions::UpdateProgress* update_progress);
     void ClearHierarchyConflicts();

     // Returns true iff conflicting_ids_ is empty. Does not check
     // encryption_conflict_ids_.
     bool no_conflicts() const;
   private:
    std::set<syncable::Id> conflicting_ids_;
    std::set<syncable::Id> successful_ids_;
    std::set<syncable::Id> encryption_conflict_ids_;
    std::set<syncable::Id> hierarchy_conflict_ids_;
  };

  // If true, AttemptOneApplication will skip over |entry| and return true.
  bool SkipUpdate(const syncable::Entry& entry);

  // Used to decrypt sensitive sync nodes.
  Cryptographer* cryptographer_;

  ModelSafeGroup group_filter_;

  const ModelSafeRoutingInfo routing_info_;

  // Track the result of the attempts to update applications.
  ResultTracker application_results_;

  DISALLOW_COPY_AND_ASSIGN(UpdateApplicator);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_UPDATE_APPLICATOR_H_
