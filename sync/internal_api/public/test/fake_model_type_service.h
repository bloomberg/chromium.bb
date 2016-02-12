// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "sync/api/data_batch.h"
#include "sync/api/entity_change.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_service.h"

namespace syncer_v2 {

// A non-functional implementation of ModelTypeService for
// testing purposes.
class FakeModelTypeService
    : public ModelTypeService,
      public base::SupportsWeakPtr<FakeModelTypeService> {
 public:
  FakeModelTypeService();
  ~FakeModelTypeService() override;

  scoped_ptr<MetadataChangeList> CreateMetadataChangeList() override;

  syncer::SyncError MergeSyncData(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityDataList entity_data_list) override;

  syncer::SyncError ApplySyncChanges(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;

  void GetData(ClientTagList client_tags, DataCallback callback) override;

  void GetAllData(DataCallback callback) override;

  std::string GetClientTag(const EntityData& entity_data) override;

  void OnChangeProcessorSet() override;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_SERVICE_H_
