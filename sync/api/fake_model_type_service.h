// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_FAKE_MODEL_TYPE_SERVICE_H_
#define SYNC_API_FAKE_MODEL_TYPE_SERVICE_H_

#include <string>

#include "sync/api/model_type_service.h"

namespace syncer_v2 {

// A non-functional implementation of ModelTypeService for
// testing purposes.
class FakeModelTypeService : public ModelTypeService {
 public:
  FakeModelTypeService();
  explicit FakeModelTypeService(
      const ChangeProcessorFactory& change_processor_factory);
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

  bool HasChangeProcessor() const;
};

}  // namespace syncer_v2

#endif  // SYNC_API_FAKE_MODEL_TYPE_SERVICE_H_
