// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_

#include <string>

#include "sync/api/data_batch.h"
#include "sync/api/entity_change.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_service.h"
#include "sync/internal_api/public/shared_model_type_processor.h"

namespace syncer_v2 {

// A non-functional implementation of ModelTypeService for
// testing purposes.
class FakeModelTypeService : public ModelTypeService {
 public:
  FakeModelTypeService();
  ~FakeModelTypeService() override;

  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;

  syncer::SyncError MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityDataMap entity_data_map) override;

  syncer::SyncError ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;

  void GetData(ClientTagList client_tags, DataCallback callback) override;

  void GetAllData(DataCallback callback) override;

  std::string GetClientTag(const EntityData& entity_data) override;

  void OnChangeProcessorSet() override;

  // TODO(gangwu): remove this and use overload constructor.
  SharedModelTypeProcessor* SetUpProcessor(ModelTypeChangeProcessor* processor);

 protected:
  // The function will create ModelTypeChangeProcessor.
  virtual ModelTypeChangeProcessor* CreateProcessorForTest(
      syncer::ModelType type,
      ModelTypeService* service);

 private:
  std::unique_ptr<ModelTypeChangeProcessor> CreateProcessorForTestWrapper(
      syncer::ModelType type,
      ModelTypeService* service);

  syncer_v2::ModelTypeChangeProcessor* processor_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_
