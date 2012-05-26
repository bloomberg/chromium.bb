// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/session_state.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/protocol/proto_enum_conversions.h"

using std::set;
using std::vector;

namespace browser_sync {
namespace sessions {

SyncSourceInfo::SyncSourceInfo()
    : updates_source(sync_pb::GetUpdatesCallerInfo::UNKNOWN) {}

SyncSourceInfo::SyncSourceInfo(
    const syncable::ModelTypePayloadMap& t)
    : updates_source(sync_pb::GetUpdatesCallerInfo::UNKNOWN), types(t) {}

SyncSourceInfo::SyncSourceInfo(
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& u,
    const syncable::ModelTypePayloadMap& t)
    : updates_source(u), types(t) {}

SyncSourceInfo::~SyncSourceInfo() {}

DictionaryValue* SyncSourceInfo::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->SetString("updatesSource",
                   GetUpdatesSourceString(updates_source));
  value->Set("types", syncable::ModelTypePayloadMapToValue(types));
  return value;
}

SyncerStatus::SyncerStatus()
    : invalid_store(false),
      num_successful_commits(0),
      num_successful_bookmark_commits(0),
      num_updates_downloaded_total(0),
      num_tombstone_updates_downloaded_total(0),
      num_reflected_updates_downloaded_total(0),
      num_local_overwrites(0),
      num_server_overwrites(0) {
}

SyncerStatus::~SyncerStatus() {
}

DictionaryValue* SyncerStatus::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->SetBoolean("invalidStore", invalid_store);
  value->SetInteger("numSuccessfulCommits", num_successful_commits);
  value->SetInteger("numSuccessfulBookmarkCommits",
                num_successful_bookmark_commits);
  value->SetInteger("numUpdatesDownloadedTotal",
                num_updates_downloaded_total);
  value->SetInteger("numTombstoneUpdatesDownloadedTotal",
                num_tombstone_updates_downloaded_total);
  value->SetInteger("numReflectedUpdatesDownloadedTotal",
                num_reflected_updates_downloaded_total);
  value->SetInteger("numLocalOverwrites", num_local_overwrites);
  value->SetInteger("numServerOverwrites", num_server_overwrites);
  return value;
}

ErrorCounters::ErrorCounters()
    : last_download_updates_result(UNSET),
      last_post_commit_result(UNSET),
      last_process_commit_response_result(UNSET) {
}

SyncSessionSnapshot::SyncSessionSnapshot()
    : num_server_changes_remaining_(0),
      is_share_usable_(false),
      has_more_to_sync_(false),
      is_silenced_(false),
      num_encryption_conflicts_(0),
      num_hierarchy_conflicts_(0),
      num_simple_conflicts_(0),
      num_server_conflicts_(0),
      notifications_enabled_(false),
      num_entries_(0),
      retry_scheduled_(false) {
}

SyncSessionSnapshot::SyncSessionSnapshot(
    const SyncerStatus& syncer_status,
    const ErrorCounters& errors,
    int64 num_server_changes_remaining,
    bool is_share_usable,
    syncable::ModelTypeSet initial_sync_ended,
    const syncable::ModelTypePayloadMap& download_progress_markers,
    bool more_to_sync,
    bool is_silenced,
    int num_encryption_conflicts,
    int num_hierarchy_conflicts,
    int num_simple_conflicts,
    int num_server_conflicts,
    const SyncSourceInfo& source,
    bool notifications_enabled,
    size_t num_entries,
    base::Time sync_start_time,
    bool retry_scheduled)
    : syncer_status_(syncer_status),
      errors_(errors),
      num_server_changes_remaining_(num_server_changes_remaining),
      is_share_usable_(is_share_usable),
      initial_sync_ended_(initial_sync_ended),
      download_progress_markers_(download_progress_markers),
      has_more_to_sync_(more_to_sync),
      is_silenced_(is_silenced),
      num_encryption_conflicts_(num_encryption_conflicts),
      num_hierarchy_conflicts_(num_hierarchy_conflicts),
      num_simple_conflicts_(num_simple_conflicts),
      num_server_conflicts_(num_server_conflicts),
      source_(source),
      notifications_enabled_(notifications_enabled),
      num_entries_(num_entries),
      sync_start_time_(sync_start_time),
      retry_scheduled_(retry_scheduled) {
}

SyncSessionSnapshot::~SyncSessionSnapshot() {}

DictionaryValue* SyncSessionSnapshot::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->Set("syncerStatus", syncer_status_.ToValue());
  // We don't care too much if we lose precision here.
  value->SetInteger("numServerChangesRemaining",
                    static_cast<int>(num_server_changes_remaining_));
  value->SetBoolean("isShareUsable", is_share_usable_);
  value->Set("initialSyncEnded",
             syncable::ModelTypeSetToValue(initial_sync_ended_));
  value->Set("downloadProgressMarkers",
             syncable::ModelTypePayloadMapToValue(download_progress_markers_));
  value->SetBoolean("hasMoreToSync", has_more_to_sync_);
  value->SetBoolean("isSilenced", is_silenced_);
  // We don't care too much if we lose precision here, also.
  value->SetInteger("numEncryptionConflicts",
                    num_encryption_conflicts_);
  value->SetInteger("numHierarchyConflicts",
                    num_hierarchy_conflicts_);
  value->SetInteger("numSimpleConflicts",
                    num_simple_conflicts_);
  value->SetInteger("numServerConflicts",
                    num_server_conflicts_);
  value->SetInteger("numEntries", num_entries_);
  value->Set("source", source_.ToValue());
  value->SetBoolean("notificationsEnabled", notifications_enabled_);
  return value;
}

std::string SyncSessionSnapshot::ToString() const {
  scoped_ptr<DictionaryValue> value(ToValue());
  std::string json;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  return json;
}

SyncerStatus SyncSessionSnapshot::syncer_status() const {
  return syncer_status_;
}

ErrorCounters SyncSessionSnapshot::errors() const {
  return errors_;
}

int64 SyncSessionSnapshot::num_server_changes_remaining() const {
  return num_server_changes_remaining_;
}

bool SyncSessionSnapshot::is_share_usable() const {
  return is_share_usable_;
}

syncable::ModelTypeSet SyncSessionSnapshot::initial_sync_ended() const {
  return initial_sync_ended_;
}

syncable::ModelTypePayloadMap
    SyncSessionSnapshot::download_progress_markers() const {
  return download_progress_markers_;
}

bool SyncSessionSnapshot::has_more_to_sync() const {
  return has_more_to_sync_;
}

bool SyncSessionSnapshot::is_silenced() const {
  return is_silenced_;
}

int SyncSessionSnapshot::num_encryption_conflicts() const {
  return num_encryption_conflicts_;
}

int SyncSessionSnapshot::num_hierarchy_conflicts() const {
  return num_hierarchy_conflicts_;
}

int SyncSessionSnapshot::num_simple_conflicts() const {
  return num_simple_conflicts_;
}

int SyncSessionSnapshot::num_server_conflicts() const {
  return num_server_conflicts_;
}

SyncSourceInfo SyncSessionSnapshot::source() const {
  return source_;
}

bool SyncSessionSnapshot::notifications_enabled() const {
  return notifications_enabled_;
}

size_t SyncSessionSnapshot::num_entries() const {
  return num_entries_;
}

base::Time SyncSessionSnapshot::sync_start_time() const {
  return sync_start_time_;
}

bool SyncSessionSnapshot::retry_scheduled() const {
  return retry_scheduled_;
}

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
}  // namespace browser_sync
