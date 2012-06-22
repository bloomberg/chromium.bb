// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace csync {
namespace sessions {

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
    const ModelNeutralState& model_neutral_state,
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
    : model_neutral_state_(model_neutral_state),
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
  value->SetInteger("numSuccessfulCommits",
                    model_neutral_state_.num_successful_commits);
  value->SetInteger("numSuccessfulBookmarkCommits",
                model_neutral_state_.num_successful_bookmark_commits);
  value->SetInteger("numUpdatesDownloadedTotal",
                model_neutral_state_.num_updates_downloaded_total);
  value->SetInteger("numTombstoneUpdatesDownloadedTotal",
                model_neutral_state_.num_tombstone_updates_downloaded_total);
  value->SetInteger("numReflectedUpdatesDownloadedTotal",
                model_neutral_state_.num_reflected_updates_downloaded_total);
  value->SetInteger("numLocalOverwrites",
                    model_neutral_state_.num_local_overwrites);
  value->SetInteger("numServerOverwrites",
                    model_neutral_state_.num_server_overwrites);
  value->SetInteger(
      "numServerChangesRemaining",
      static_cast<int>(model_neutral_state_.num_server_changes_remaining));
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

int64 SyncSessionSnapshot::num_server_changes_remaining() const {
  return model_neutral_state().num_server_changes_remaining;
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

}  // namespace sessions
}  // namespace csync
