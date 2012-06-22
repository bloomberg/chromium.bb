// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/status_controller.h"

#include <vector>

#include "base/basictypes.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/protocol/sync_protocol_error.h"

namespace csync {
namespace sessions {

using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;

StatusController::StatusController(const ModelSafeRoutingInfo& routes)
    : per_model_group_deleter_(&per_model_group_),
      group_restriction_in_effect_(false),
      group_restriction_(GROUP_PASSIVE),
      routing_info_(routes) {
}

StatusController::~StatusController() {}

const UpdateProgress* StatusController::update_progress() const {
  const PerModelSafeGroupState* state =
      GetModelSafeGroupState(true, group_restriction_);
  return state ? &state->update_progress : NULL;
}

UpdateProgress* StatusController::mutable_update_progress() {
  return &GetOrCreateModelSafeGroupState(
      true, group_restriction_)->update_progress;
}

const ConflictProgress* StatusController::conflict_progress() const {
  const PerModelSafeGroupState* state =
      GetModelSafeGroupState(true, group_restriction_);
  return state ? &state->conflict_progress : NULL;
}

ConflictProgress* StatusController::mutable_conflict_progress() {
  return &GetOrCreateModelSafeGroupState(
      true, group_restriction_)->conflict_progress;
}

const ConflictProgress* StatusController::GetUnrestrictedConflictProgress(
    ModelSafeGroup group) const {
  const PerModelSafeGroupState* state =
      GetModelSafeGroupState(false, group);
  return state ? &state->conflict_progress : NULL;
}

ConflictProgress*
    StatusController::GetUnrestrictedMutableConflictProgressForTest(
        ModelSafeGroup group) {
  return &GetOrCreateModelSafeGroupState(false, group)->conflict_progress;
}

const UpdateProgress* StatusController::GetUnrestrictedUpdateProgress(
    ModelSafeGroup group) const {
  const PerModelSafeGroupState* state =
      GetModelSafeGroupState(false, group);
  return state ? &state->update_progress : NULL;
}

UpdateProgress*
    StatusController::GetUnrestrictedMutableUpdateProgressForTest(
        ModelSafeGroup group) {
  return &GetOrCreateModelSafeGroupState(false, group)->update_progress;
}

const PerModelSafeGroupState* StatusController::GetModelSafeGroupState(
    bool restrict, ModelSafeGroup group) const {
  DCHECK_EQ(restrict, group_restriction_in_effect_);
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
      per_model_group_.find(group);
  return (it == per_model_group_.end()) ? NULL : it->second;
}

PerModelSafeGroupState* StatusController::GetOrCreateModelSafeGroupState(
    bool restrict, ModelSafeGroup group) {
  DCHECK_EQ(restrict, group_restriction_in_effect_);
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::iterator it =
      per_model_group_.find(group);
  if (it == per_model_group_.end()) {
    PerModelSafeGroupState* state = new PerModelSafeGroupState();
    it = per_model_group_.insert(std::make_pair(group, state)).first;
  }
  return it->second;
}

void StatusController::increment_num_updates_downloaded_by(int value) {
  model_neutral_.num_updates_downloaded_total += value;
}

void StatusController::set_types_needing_local_migration(
    syncable::ModelTypeSet types) {
  model_neutral_.types_needing_local_migration = types;
}

void StatusController::increment_num_tombstone_updates_downloaded_by(
    int value) {
  model_neutral_.num_tombstone_updates_downloaded_total += value;
}

void StatusController::increment_num_reflected_updates_downloaded_by(
    int value) {
  model_neutral_.num_reflected_updates_downloaded_total += value;
}

void StatusController::set_num_server_changes_remaining(
    int64 changes_remaining) {
  model_neutral_.num_server_changes_remaining = changes_remaining;
}

void StatusController::UpdateStartTime() {
  sync_start_time_ = base::Time::Now();
}

void StatusController::set_num_successful_bookmark_commits(int value) {
  model_neutral_.num_successful_bookmark_commits = value;
}

void StatusController::increment_num_successful_bookmark_commits() {
  model_neutral_.num_successful_bookmark_commits++;
}

void StatusController::increment_num_successful_commits() {
  model_neutral_.num_successful_commits++;
}

void StatusController::increment_num_local_overwrites() {
  model_neutral_.num_local_overwrites++;
}

void StatusController::increment_num_server_overwrites() {
  model_neutral_.num_server_overwrites++;
}

void StatusController::set_sync_protocol_error(
    const SyncProtocolError& error) {
  model_neutral_.sync_protocol_error = error;
}

void StatusController::set_last_download_updates_result(
    const SyncerError result) {
  model_neutral_.last_download_updates_result = result;
}

void StatusController::set_commit_result(const SyncerError result) {
  model_neutral_.commit_result = result;
}

void StatusController::update_conflicts_resolved(bool resolved) {
  model_neutral_.conflicts_resolved |= resolved;
}
void StatusController::reset_conflicts_resolved() {
  model_neutral_.conflicts_resolved = false;
}

// Returns the number of updates received from the sync server.
int64 StatusController::CountUpdates() const {
  const ClientToServerResponse& updates = model_neutral_.updates_response;
  if (updates.has_get_updates()) {
    return updates.get_updates().entries().size();
  } else {
    return 0;
  }
}

bool StatusController::HasConflictingUpdates() const {
  DCHECK(!group_restriction_in_effect_)
      << "HasConflictingUpdates applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
    per_model_group_.begin();
  for (; it != per_model_group_.end(); ++it) {
    if (it->second->update_progress.HasConflictingUpdates())
      return true;
  }
  return false;
}

int StatusController::TotalNumEncryptionConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumEncryptionConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
      per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.EncryptionConflictingItemsSize();
  }
  return sum;
}

int StatusController::TotalNumHierarchyConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumHierarchyConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
      per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.HierarchyConflictingItemsSize();
  }
  return sum;
}

int StatusController::TotalNumSimpleConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumSimpleConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
      per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.SimpleConflictingItemsSize();
  }
  return sum;
}

int StatusController::TotalNumServerConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumServerConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
      per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.ServerConflictingItemsSize();
  }
  return sum;
}

int StatusController::TotalNumConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
    per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.SimpleConflictingItemsSize();
    sum += it->second->conflict_progress.EncryptionConflictingItemsSize();
    sum += it->second->conflict_progress.HierarchyConflictingItemsSize();
    sum += it->second->conflict_progress.ServerConflictingItemsSize();
  }
  return sum;
}

bool StatusController::ServerSaysNothingMoreToDownload() const {
  if (!download_updates_succeeded())
    return false;

  if (!updates_response().get_updates().has_changes_remaining()) {
    NOTREACHED();  // Server should always send changes remaining.
    return false;  // Avoid looping forever.
  }
  // Changes remaining is an estimate, but if it's estimated to be
  // zero, that's firm and we don't have to ask again.
  return updates_response().get_updates().changes_remaining() == 0;
}

void StatusController::set_debug_info_sent() {
  model_neutral_.debug_info_sent = true;
}

bool StatusController::debug_info_sent() const {
  return model_neutral_.debug_info_sent;
}

}  // namespace sessions
}  // namespace csync
