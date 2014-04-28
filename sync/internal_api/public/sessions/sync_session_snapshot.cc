// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/protocol/proto_enum_conversions.h"

namespace syncer {
namespace sessions {

SyncSessionSnapshot::SyncSessionSnapshot()
    : is_silenced_(false),
      num_encryption_conflicts_(0),
      num_hierarchy_conflicts_(0),
      num_server_conflicts_(0),
      notifications_enabled_(false),
      num_entries_(0),
      num_entries_by_type_(MODEL_TYPE_COUNT, 0),
      num_to_delete_entries_by_type_(MODEL_TYPE_COUNT, 0),
      is_initialized_(false) {
}

SyncSessionSnapshot::SyncSessionSnapshot(
    const ModelNeutralState& model_neutral_state,
    const ProgressMarkerMap& download_progress_markers,
    bool is_silenced,
    int num_encryption_conflicts,
    int num_hierarchy_conflicts,
    int num_server_conflicts,
    bool notifications_enabled,
    size_t num_entries,
    base::Time sync_start_time,
    const std::vector<int>& num_entries_by_type,
    const std::vector<int>& num_to_delete_entries_by_type,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource legacy_updates_source)
    : model_neutral_state_(model_neutral_state),
      download_progress_markers_(download_progress_markers),
      is_silenced_(is_silenced),
      num_encryption_conflicts_(num_encryption_conflicts),
      num_hierarchy_conflicts_(num_hierarchy_conflicts),
      num_server_conflicts_(num_server_conflicts),
      notifications_enabled_(notifications_enabled),
      num_entries_(num_entries),
      sync_start_time_(sync_start_time),
      num_entries_by_type_(num_entries_by_type),
      num_to_delete_entries_by_type_(num_to_delete_entries_by_type),
      legacy_updates_source_(legacy_updates_source),
      is_initialized_(true) {
}

SyncSessionSnapshot::~SyncSessionSnapshot() {}

base::DictionaryValue* SyncSessionSnapshot::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
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
  value->Set("downloadProgressMarkers",
             ProgressMarkerMapToValue(download_progress_markers_).release());
  value->SetBoolean("isSilenced", is_silenced_);
  // We don't care too much if we lose precision here, also.
  value->SetInteger("numEncryptionConflicts",
                    num_encryption_conflicts_);
  value->SetInteger("numHierarchyConflicts",
                    num_hierarchy_conflicts_);
  value->SetInteger("numServerConflicts",
                    num_server_conflicts_);
  value->SetInteger("numEntries", num_entries_);
  value->SetString("legacySource",
                   GetUpdatesSourceString(legacy_updates_source_));
  value->SetBoolean("notificationsEnabled", notifications_enabled_);

  scoped_ptr<base::DictionaryValue> counter_entries(
      new base::DictionaryValue());
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; i++) {
    scoped_ptr<base::DictionaryValue> type_entries(new base::DictionaryValue());
    type_entries->SetInteger("numEntries", num_entries_by_type_[i]);
    type_entries->SetInteger("numToDeleteEntries",
                             num_to_delete_entries_by_type_[i]);

    const std::string model_type = ModelTypeToString(static_cast<ModelType>(i));
    counter_entries->Set(model_type, type_entries.release());
  }
  value->Set("counter_entries", counter_entries.release());
  return value.release();
}

std::string SyncSessionSnapshot::ToString() const {
  scoped_ptr<base::DictionaryValue> value(ToValue());
  std::string json;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  return json;
}

const ProgressMarkerMap&
    SyncSessionSnapshot::download_progress_markers() const {
  return download_progress_markers_;
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

int SyncSessionSnapshot::num_server_conflicts() const {
  return num_server_conflicts_;
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

bool SyncSessionSnapshot::is_initialized() const {
  return is_initialized_;
}

const std::vector<int>& SyncSessionSnapshot::num_entries_by_type() const {
  return num_entries_by_type_;
}

const std::vector<int>&
SyncSessionSnapshot::num_to_delete_entries_by_type() const {
  return num_to_delete_entries_by_type_;
}

sync_pb::GetUpdatesCallerInfo::GetUpdatesSource
SyncSessionSnapshot::legacy_updates_source() const {
  return legacy_updates_source_;
}

}  // namespace sessions
}  // namespace syncer
