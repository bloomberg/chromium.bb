// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define SYNC_API_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <string>

#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_change_processor.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer_v2 {

class ModelTypeService;

// A ModelTypeChangeProcessor implementation for tests.
class FakeModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  static std::unique_ptr<ModelTypeChangeProcessor> Create(
      syncer::ModelType type,
      ModelTypeService* service);

  FakeModelTypeChangeProcessor();
  ~FakeModelTypeChangeProcessor() override;

  // ModelTypeChangeProcessor overrides
  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void OnMetadataLoaded(std::unique_ptr<MetadataBatch> batch) override;
  void OnSyncStarting(const StartCallback& callback) override;
  void DisableSync() override;
};

}  // namespace syncer_v2

#endif  // SYNC_API_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
