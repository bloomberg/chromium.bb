// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_model_type_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "sync/internal_api/public/shared_model_type_processor.h"

namespace syncer_v2 {

FakeModelTypeService::FakeModelTypeService()
    : ModelTypeService(
          base::Bind(&FakeModelTypeService::CreateProcessorForTestWrapper,
                     base::Unretained(this)),
          syncer::PREFERENCES),
      processor_(nullptr) {}

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

ModelTypeChangeProcessor* FakeModelTypeService::CreateProcessorForTest(
    syncer::ModelType type,
    ModelTypeService* service) {
  return processor_;
}

std::unique_ptr<ModelTypeChangeProcessor>
FakeModelTypeService::CreateProcessorForTestWrapper(syncer::ModelType type,
                                                    ModelTypeService* service) {
  return base::WrapUnique(CreateProcessorForTest(type, service));
}

SharedModelTypeProcessor* FakeModelTypeService::SetUpProcessor(
    ModelTypeChangeProcessor* processor) {
  processor_ = processor;
  return static_cast<SharedModelTypeProcessor*>(GetOrCreateChangeProcessor());
}

}  // namespace syncer_v2
