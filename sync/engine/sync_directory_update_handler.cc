// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_update_handler.h"

#include "sync/engine/process_updates_util.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"

namespace syncer {

using syncable::SYNCER;

SyncDirectoryUpdateHandler::SyncDirectoryUpdateHandler(
    syncable::Directory* dir, ModelType type)
  : dir_(dir),
    type_(type) {}

SyncDirectoryUpdateHandler::~SyncDirectoryUpdateHandler() {}

void SyncDirectoryUpdateHandler::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  dir_->GetDownloadProgress(type_, progress_marker);
}

void SyncDirectoryUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  UpdateSyncEntities(&trans, applicable_updates, status);
  UpdateProgressMarker(progress_marker);
}

void SyncDirectoryUpdateHandler::UpdateSyncEntities(
    syncable::ModelNeutralWriteTransaction* trans,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  ProcessDownloadedUpdates(dir_, trans, type_, applicable_updates, status);
}

void SyncDirectoryUpdateHandler::UpdateProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  int field_number = progress_marker.data_type_id();
  ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
  if (!IsRealDataType(model_type) || type_ != model_type) {
    NOTREACHED()
        << "Update handler of type " << ModelTypeToString(type_)
        << " asked to process progress marker with invalid type "
        << field_number;
  }
  dir_->SetDownloadProgress(type_, progress_marker);
}

}  // namespace syncer
