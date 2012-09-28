// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/update_applicator.h"

#include <vector>

#include "base/logging.h"
#include "sync/engine/syncer_util.h"
#include "sync/sessions/session_state.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/write_transaction.h"

using std::vector;

namespace syncer {

using syncable::ID;

UpdateApplicator::UpdateApplicator(Cryptographer* cryptographer,
                                   const ModelSafeRoutingInfo& routes,
                                   ModelSafeGroup group_filter)
    : cryptographer_(cryptographer),
      group_filter_(group_filter),
      routing_info_(routes),
      application_results_() {
}

UpdateApplicator::~UpdateApplicator() {
}

// Attempt to apply all updates, using multiple passes if necessary.
//
// Some updates must be applied in order.  For example, children must be created
// after their parent folder is created.  This function runs an O(n^2) algorithm
// that will keep trying until there is nothing left to apply, or it stops
// making progress, which would indicate that the hierarchy is invalid.
//
// The update applicator also has to deal with simple conflicts, which occur
// when an item is modified on both the server and the local model, and
// encryption conflicts.  There's not much we can do about them here, so we
// don't bother re-processing them on subsequent passes.
void UpdateApplicator::AttemptApplications(
    syncable::WriteTransaction* trans,
    const std::vector<int64>& handles) {
  std::vector<int64> to_apply = handles;
  DVLOG(1) << "UpdateApplicator running over " << to_apply.size() << " items.";
  while (!to_apply.empty()) {
    std::vector<int64> to_reapply;

    for (UpdateIterator i = to_apply.begin(); i != to_apply.end(); ++i) {
      syncable::Entry read_entry(trans, syncable::GET_BY_HANDLE, *i);
      if (SkipUpdate(read_entry)) {
        continue;
      }

      syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *i);
      UpdateAttemptResponse result = AttemptToUpdateEntry(
          trans, &entry, cryptographer_);

      switch (result) {
        case SUCCESS:
          application_results_.AddSuccess(entry.Get(ID));
          break;
        case CONFLICT_SIMPLE:
          application_results_.AddSimpleConflict(entry.Get(ID));
          break;
        case CONFLICT_ENCRYPTION:
          application_results_.AddEncryptionConflict(entry.Get(ID));
          break;
        case CONFLICT_HIERARCHY:
          application_results_.AddHierarchyConflict(entry.Get(ID));
          // The decision to classify these as hierarchy conflcits is tentative.
          // If we make any progress this round, we'll clear the hierarchy
          // conflict count and attempt to reapply these updates.
          to_reapply.push_back(*i);
          break;
        default:
          NOTREACHED();
          break;
      }
    }

    if (to_reapply.size() == to_apply.size()) {
      // We made no progress.  Must be stubborn hierarchy conflicts.
      // Break out early, leaving some updates unapplied.
      break;
    }

    // We made some progress, so prepare for what might be another iteration.
    // If everything went well, to_reapply will be empty and we'll break out on
    // the while condition.
    application_results_.ClearHierarchyConflicts();
    to_apply.swap(to_reapply);
    to_reapply.clear();
  }
}

bool UpdateApplicator::SkipUpdate(const syncable::Entry& entry) {
  ModelType type = entry.GetServerModelType();
  ModelSafeGroup g = GetGroupForModelType(type, routing_info_);
  // The set of updates passed to the UpdateApplicator should already
  // be group-filtered.
  if (g != group_filter_) {
    NOTREACHED();
    return true;
  }
  if (g == GROUP_PASSIVE &&
      !routing_info_.count(type) &&
      type != UNSPECIFIED &&
      type != TOP_LEVEL_FOLDER) {
    DVLOG(1) << "Skipping update application, type not permitted.";
    return true;
  }
  return false;
}

void UpdateApplicator::SaveProgressIntoSessionState(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  application_results_.SaveProgress(conflict_progress, update_progress);
}

UpdateApplicator::ResultTracker::ResultTracker() {
}

UpdateApplicator::ResultTracker::~ResultTracker() {
}

void UpdateApplicator::ResultTracker::AddSimpleConflict(syncable::Id id) {
  conflicting_ids_.insert(id);
}

void UpdateApplicator::ResultTracker::AddEncryptionConflict(syncable::Id id) {
  encryption_conflict_ids_.insert(id);
}

void UpdateApplicator::ResultTracker::AddHierarchyConflict(syncable::Id id) {
  hierarchy_conflict_ids_.insert(id);
}

void UpdateApplicator::ResultTracker::AddSuccess(syncable::Id id) {
  successful_ids_.insert(id);
}

void UpdateApplicator::ResultTracker::SaveProgress(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  std::set<syncable::Id>::const_iterator i;
  for (i = conflicting_ids_.begin(); i != conflicting_ids_.end(); ++i) {
    conflict_progress->AddSimpleConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT_SIMPLE, *i);
  }
  for (i = encryption_conflict_ids_.begin();
       i != encryption_conflict_ids_.end(); ++i) {
    conflict_progress->AddEncryptionConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT_ENCRYPTION, *i);
  }
  for (i = hierarchy_conflict_ids_.begin();
       i != hierarchy_conflict_ids_.end(); ++i) {
    conflict_progress->AddHierarchyConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT_HIERARCHY, *i);
  }
  for (i = successful_ids_.begin(); i != successful_ids_.end(); ++i) {
    conflict_progress->EraseSimpleConflictingItemById(*i);
    update_progress->AddAppliedUpdate(SUCCESS, *i);
  }
}

void UpdateApplicator::ResultTracker::ClearHierarchyConflicts() {
  hierarchy_conflict_ids_.clear();
}

bool UpdateApplicator::ResultTracker::no_conflicts() const {
  return conflicting_ids_.empty();
}

}  // namespace syncer
