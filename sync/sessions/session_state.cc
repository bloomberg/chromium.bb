// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/session_state.h"

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

using std::set;
using std::vector;

namespace csync {
namespace sessions {

ConflictProgress::ConflictProgress(bool* dirty_flag)
  : num_server_conflicting_items(0), num_hierarchy_conflicting_items(0),
    num_encryption_conflicting_items(0), dirty_(dirty_flag) {
}

ConflictProgress::~ConflictProgress() {
}

bool ConflictProgress::HasSimpleConflictItem(const syncable::Id& id) const {
  return simple_conflicting_item_ids_.count(id) > 0;
}

std::set<syncable::Id>::const_iterator
ConflictProgress::SimpleConflictingItemsBegin() const {
  return simple_conflicting_item_ids_.begin();
}
std::set<syncable::Id>::const_iterator
ConflictProgress::SimpleConflictingItemsEnd() const {
  return simple_conflicting_item_ids_.end();
}

void ConflictProgress::AddSimpleConflictingItemById(
    const syncable::Id& the_id) {
  std::pair<std::set<syncable::Id>::iterator, bool> ret =
      simple_conflicting_item_ids_.insert(the_id);
  if (ret.second)
    *dirty_ = true;
}

void ConflictProgress::EraseSimpleConflictingItemById(
    const syncable::Id& the_id) {
  int items_erased = simple_conflicting_item_ids_.erase(the_id);
  if (items_erased != 0)
    *dirty_ = true;
}

void ConflictProgress::AddEncryptionConflictingItemById(
    const syncable::Id& the_id) {
  std::pair<std::set<syncable::Id>::iterator, bool> ret =
      unresolvable_conflicting_item_ids_.insert(the_id);
  if (ret.second) {
    num_encryption_conflicting_items++;
    *dirty_ = true;
  }
}

void ConflictProgress::AddHierarchyConflictingItemById(
    const syncable::Id& the_id) {
  std::pair<std::set<syncable::Id>::iterator, bool> ret =
      unresolvable_conflicting_item_ids_.insert(the_id);
  if (ret.second) {
    num_hierarchy_conflicting_items++;
    *dirty_ = true;
  }
}

void ConflictProgress::AddServerConflictingItemById(
    const syncable::Id& the_id) {
  std::pair<std::set<syncable::Id>::iterator, bool> ret =
      unresolvable_conflicting_item_ids_.insert(the_id);
  if (ret.second) {
    num_server_conflicting_items++;
    *dirty_ = true;
  }
}

UpdateProgress::UpdateProgress() {}

UpdateProgress::~UpdateProgress() {}

void UpdateProgress::AddVerifyResult(const VerifyResult& verify_result,
                                     const sync_pb::SyncEntity& entity) {
  verified_updates_.push_back(std::make_pair(verify_result, entity));
}

void UpdateProgress::AddAppliedUpdate(const UpdateAttemptResponse& response,
    const syncable::Id& id) {
  applied_updates_.push_back(std::make_pair(response, id));
}

std::vector<AppliedUpdate>::iterator UpdateProgress::AppliedUpdatesBegin() {
  return applied_updates_.begin();
}

std::vector<VerifiedUpdate>::const_iterator
UpdateProgress::VerifiedUpdatesBegin() const {
  return verified_updates_.begin();
}

std::vector<AppliedUpdate>::const_iterator
UpdateProgress::AppliedUpdatesEnd() const {
  return applied_updates_.end();
}

std::vector<VerifiedUpdate>::const_iterator
UpdateProgress::VerifiedUpdatesEnd() const {
  return verified_updates_.end();
}

int UpdateProgress::SuccessfullyAppliedUpdateCount() const {
  int count = 0;
  for (std::vector<AppliedUpdate>::const_iterator it =
       applied_updates_.begin();
       it != applied_updates_.end();
       ++it) {
    if (it->first == SUCCESS)
      count++;
  }
  return count;
}

// Returns true if at least one update application failed due to a conflict
// during this sync cycle.
bool UpdateProgress::HasConflictingUpdates() const {
  std::vector<AppliedUpdate>::const_iterator it;
  for (it = applied_updates_.begin(); it != applied_updates_.end(); ++it) {
    if (it->first != SUCCESS) {
      return true;
    }
  }
  return false;
}

AllModelTypeState::AllModelTypeState(bool* dirty_flag)
    : syncer_status(dirty_flag),
      error(dirty_flag),
      num_server_changes_remaining(dirty_flag, 0) {
}

AllModelTypeState::~AllModelTypeState() {}

PerModelSafeGroupState::PerModelSafeGroupState(bool* dirty_flag)
    : conflict_progress(dirty_flag) {
}

PerModelSafeGroupState::~PerModelSafeGroupState() {
}

}  // namespace sessions
}  // namespace csync
