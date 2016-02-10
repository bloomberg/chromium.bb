// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_model_type_service.h"

namespace syncer_v2 {

FakeModelTypeService::FakeModelTypeService() {}

FakeModelTypeService::~FakeModelTypeService() {}

scoped_ptr<MetadataChangeList>
FakeModelTypeService::CreateMetadataChangeList() {
  return scoped_ptr<MetadataChangeList>();
}

syncer::SyncError FakeModelTypeService::MergeSyncData(
    scoped_ptr<MetadataChangeList> metadata_change_list,
    EntityDataList entity_data_list) {
  return syncer::SyncError();
}

syncer::SyncError FakeModelTypeService::ApplySyncChanges(
    scoped_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return syncer::SyncError();
}

void FakeModelTypeService::GetData(ClientTagList client_tags,
                                   DataCallback callback) {}

void FakeModelTypeService::GetAllData(DataCallback callback) {}

std::string FakeModelTypeService::GetClientTag(const EntityData& entity_data) {
  return std::string();
}

}  // namespace syncer_v2
