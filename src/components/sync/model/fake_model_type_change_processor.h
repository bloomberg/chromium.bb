// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"

namespace syncer {

class ModelTypeSyncBridge;

// A ModelTypeChangeProcessor implementation for tests.
class FakeModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  FakeModelTypeChangeProcessor();
  explicit FakeModelTypeChangeProcessor(
      base::WeakPtr<ModelTypeControllerDelegate> delegate);
  ~FakeModelTypeChangeProcessor() override;

  // ModelTypeChangeProcessor overrides
  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override;
  void UntrackEntity(const EntityData& entity_data) override;
  void UntrackEntityForStorageKey(const std::string& storage_key) override;
  void OnModelStarting(ModelTypeSyncBridge* bridge) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() override;
  void ReportError(const ModelError& error) override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override;

  // Indicates that ReportError should be called in the future.
  void ExpectError();

 private:
  // Whether we expect ReportError to be called.
  bool expect_error_ = false;
  base::WeakPtr<ModelTypeControllerDelegate> delegate_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
