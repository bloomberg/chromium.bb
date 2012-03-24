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

DictionaryValue* DownloadProgressMarkersToValue(
    const std::string
        (&download_progress_markers)[syncable::MODEL_TYPE_COUNT]) {
  DictionaryValue* value = new DictionaryValue();
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    // TODO(akalin): Unpack the value into a protobuf.
    std::string base64_marker;
    bool encoded =
        base::Base64Encode(download_progress_markers[i], &base64_marker);
    DCHECK(encoded);
    value->SetString(
        syncable::ModelTypeToString(syncable::ModelTypeFromInt(i)),
        base64_marker);
  }
  return value;
}

ErrorCounters::ErrorCounters()
    : last_download_updates_result(UNSET),
      last_post_commit_result(UNSET),
      last_process_commit_response_result(UNSET) {
}

SyncSessionSnapshot::SyncSessionSnapshot(
    const SyncerStatus& syncer_status,
    const ErrorCounters& errors,
    int64 num_server_changes_remaining,
    bool is_share_usable,
    syncable::ModelTypeSet initial_sync_ended,
    const std::string
        (&download_progress_markers)[syncable::MODEL_TYPE_COUNT],
    bool more_to_sync,
    bool is_silenced,
    int64 unsynced_count,
    int num_encryption_conflicts,
    int num_hierarchy_conflicts,
    int num_simple_conflicts,
    int num_server_conflicts,
    bool did_commit_items,
    const SyncSourceInfo& source,
    bool notifications_enabled,
    size_t num_entries,
    base::Time sync_start_time,
    bool retry_scheduled)
    : syncer_status(syncer_status),
      errors(errors),
      num_server_changes_remaining(num_server_changes_remaining),
      is_share_usable(is_share_usable),
      initial_sync_ended(initial_sync_ended),
      download_progress_markers(),
      has_more_to_sync(more_to_sync),
      is_silenced(is_silenced),
      unsynced_count(unsynced_count),
      num_encryption_conflicts(num_encryption_conflicts),
      num_hierarchy_conflicts(num_hierarchy_conflicts),
      num_simple_conflicts(num_simple_conflicts),
      num_server_conflicts(num_server_conflicts),
      did_commit_items(did_commit_items),
      source(source),
      notifications_enabled(notifications_enabled),
      num_entries(num_entries),
      sync_start_time(sync_start_time),
      retry_scheduled(retry_scheduled) {
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const_cast<std::string&>(this->download_progress_markers[i]).assign(
        download_progress_markers[i]);
  }
}

SyncSessionSnapshot::~SyncSessionSnapshot() {}

DictionaryValue* SyncSessionSnapshot::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->Set("syncerStatus", syncer_status.ToValue());
  // We don't care too much if we lose precision here.
  value->SetInteger("numServerChangesRemaining",
                    static_cast<int>(num_server_changes_remaining));
  value->SetBoolean("isShareUsable", is_share_usable);
  value->Set("initialSyncEnded",
             syncable::ModelTypeSetToValue(initial_sync_ended));
  value->Set("downloadProgressMarkers",
             DownloadProgressMarkersToValue(download_progress_markers));
  value->SetBoolean("hasMoreToSync", has_more_to_sync);
  value->SetBoolean("isSilenced", is_silenced);
  // We don't care too much if we lose precision here, also.
  value->SetInteger("unsyncedCount",
                    static_cast<int>(unsynced_count));
  value->SetInteger("numEncryptionConflicts",
                    num_encryption_conflicts);
  value->SetInteger("numHierarchyConflicts",
                    num_hierarchy_conflicts);
  value->SetInteger("numSimpleConflicts",
                    num_simple_conflicts);
  value->SetInteger("numServerConflicts",
                    num_server_conflicts);
  value->SetBoolean("didCommitItems", did_commit_items);
  value->SetInteger("numEntries", num_entries);
  value->Set("source", source.ToValue());
  value->SetBoolean("notificationsEnabled", notifications_enabled);
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
    : unsynced_handles(dirty_flag),
      syncer_status(dirty_flag),
      error(dirty_flag),
      num_server_changes_remaining(dirty_flag, 0),
      commit_set(ModelSafeRoutingInfo()) {
}

AllModelTypeState::~AllModelTypeState() {}

PerModelSafeGroupState::PerModelSafeGroupState(bool* dirty_flag)
    : conflict_progress(dirty_flag) {
}

PerModelSafeGroupState::~PerModelSafeGroupState() {
}

}  // namespace sessions
}  // namespace browser_sync
