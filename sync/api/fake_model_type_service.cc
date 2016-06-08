// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/fake_model_type_service.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "sync/api/fake_model_type_change_processor.h"

namespace syncer_v2 {

FakeModelTypeService::FakeModelTypeService()
    : FakeModelTypeService(base::Bind(&FakeModelTypeChangeProcessor::Create)) {}

FakeModelTypeService::FakeModelTypeService(
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeService(change_processor_factory, syncer::PREFERENCES) {}

FakeModelTypeService::~FakeModelTypeService() {}

std::unique_ptr<MetadataChangeList>
FakeModelTypeService::CreateMetadataChangeList() {
  return std::unique_ptr<MetadataChangeList>();
}

syncer::SyncError FakeModelTypeService::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  return syncer::SyncError();
}

syncer::SyncError FakeModelTypeService::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return syncer::SyncError();
}

void FakeModelTypeService::GetData(ClientTagList client_tags,
                                   DataCallback callback) {}

void FakeModelTypeService::GetAllData(DataCallback callback) {}

std::string FakeModelTypeService::GetClientTag(const EntityData& entity_data) {
  return std::string();
}

void FakeModelTypeService::OnChangeProcessorSet() {}

bool FakeModelTypeService::HasChangeProcessor() const {
  return change_processor() != nullptr;
}

}  // namespace syncer_v2
