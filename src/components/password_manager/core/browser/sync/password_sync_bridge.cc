// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include "base/logging.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"

namespace password_manager {

PasswordSyncBridge::PasswordSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

PasswordSyncBridge::~PasswordSyncBridge() = default;

void PasswordSyncBridge::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<syncer::MetadataChangeList>
PasswordSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void PasswordSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  NOTIMPLEMENTED();
}

void PasswordSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string PasswordSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

std::string PasswordSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

syncer::ModelTypeSyncBridge::StopSyncResponse
PasswordSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
  return ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync;
}

}  // namespace password_manager
