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

namespace browser_sync {

UpdateApplicator::UpdateApplicator(ConflictResolver* resolver,
                                   Cryptographer* cryptographer,
                                   const UpdateIterator& begin,
                                   const UpdateIterator& end,
                                   const ModelSafeRoutingInfo& routes,
                                   ModelSafeGroup group_filter)
    : resolver_(resolver),
      cryptographer_(cryptographer),
      begin_(begin),
      end_(end),
      pointer_(begin),
      group_filter_(group_filter),
      progress_(false),
      routing_info_(routes),
      application_results_(end - begin) {
  size_t item_count = end - begin;
  DVLOG(1) << "UpdateApplicator created for " << item_count << " items.";
}

UpdateApplicator::~UpdateApplicator() {
}

// Returns true if there's more to do.
bool UpdateApplicator::AttemptOneApplication(
    syncable::WriteTransaction* trans) {
  // If there are no updates left to consider, we're done.
  if (end_ == begin_)
    return false;
  if (pointer_ == end_) {
    if (!progress_)
      return false;

    DVLOG(1) << "UpdateApplicator doing additional pass.";
    pointer_ = begin_;
    progress_ = false;

    // Clear the tracked failures to avoid double-counting.
    application_results_.ClearConflicts();
  }

  syncable::Entry read_only(trans, syncable::GET_BY_HANDLE, *pointer_);
  if (SkipUpdate(read_only)) {
    Advance();
    return true;
  }

  syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *pointer_);
  UpdateAttemptResponse updateResponse = SyncerUtil::AttemptToUpdateEntry(
      trans, &entry, resolver_, cryptographer_);
  switch (updateResponse) {
    case SUCCESS:
      Advance();
      progress_ = true;
      application_results_.AddSuccess(entry.Get(syncable::ID));
      break;
    case CONFLICT_SIMPLE:
      pointer_++;
      application_results_.AddSimpleConflict(entry.Get(syncable::ID));
      break;
    case CONFLICT_ENCRYPTION:
      pointer_++;
      application_results_.AddEncryptionConflict(entry.Get(syncable::ID));
      break;
    case CONFLICT_HIERARCHY:
      pointer_++;
      application_results_.AddHierarchyConflict(entry.Get(syncable::ID));
      break;
    default:
      NOTREACHED();
      break;
  }
  DVLOG(1) << "Apply Status for " << entry.Get(syncable::META_HANDLE)
           << " is " << updateResponse;

  return true;
}

void UpdateApplicator::Advance() {
  --end_;
  *pointer_ = *end_;
}

bool UpdateApplicator::SkipUpdate(const syncable::Entry& entry) {
  syncable::ModelType type = entry.GetServerModelType();
  ModelSafeGroup g = GetGroupForModelType(type, routing_info_);
  // The set of updates passed to the UpdateApplicator should already
  // be group-filtered.
  if (g != group_filter_) {
    NOTREACHED();
    return true;
  }
  if (g == GROUP_PASSIVE &&
      !routing_info_.count(type) &&
      type != syncable::UNSPECIFIED &&
      type != syncable::TOP_LEVEL_FOLDER) {
    DVLOG(1) << "Skipping update application, type not permitted.";
    return true;
  }
  return false;
}

bool UpdateApplicator::AllUpdatesApplied() const {
  return application_results_.no_conflicts() && begin_ == end_;
}

void UpdateApplicator::SaveProgressIntoSessionState(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  DCHECK(begin_ == end_ || ((pointer_ == end_) && !progress_))
      << "SaveProgress called before updates exhausted.";

  application_results_.SaveProgress(conflict_progress, update_progress);
}

UpdateApplicator::ResultTracker::ResultTracker(size_t num_results) {
  successful_ids_.reserve(num_results);
}

UpdateApplicator::ResultTracker::~ResultTracker() {
}

void UpdateApplicator::ResultTracker::AddSimpleConflict(syncable::Id id) {
  conflicting_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::AddEncryptionConflict(syncable::Id id) {
  encryption_conflict_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::AddHierarchyConflict(syncable::Id id) {
  hierarchy_conflict_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::AddSuccess(syncable::Id id) {
  successful_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::SaveProgress(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  vector<syncable::Id>::const_iterator i;
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

void UpdateApplicator::ResultTracker::ClearConflicts() {
  conflicting_ids_.clear();
  encryption_conflict_ids_.clear();
  hierarchy_conflict_ids_.clear();
}

bool UpdateApplicator::ResultTracker::no_conflicts() const {
  return conflicting_ids_.empty();
}

}  // namespace browser_sync
