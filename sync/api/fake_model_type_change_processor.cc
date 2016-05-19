// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/fake_model_type_change_processor.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_service.h"
#include "sync/api/sync_error.h"

namespace syncer_v2 {

// static
std::unique_ptr<ModelTypeChangeProcessor> FakeModelTypeChangeProcessor::Create(
    syncer::ModelType type,
    ModelTypeService* service) {
  return base::WrapUnique(new FakeModelTypeChangeProcessor());
}

FakeModelTypeChangeProcessor::FakeModelTypeChangeProcessor() {}
FakeModelTypeChangeProcessor::~FakeModelTypeChangeProcessor() {}

void FakeModelTypeChangeProcessor::Put(
    const std::string& client_tag,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::Delete(
    const std::string& client_tag,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::OnMetadataLoaded(
    syncer::SyncError error,
    std::unique_ptr<MetadataBatch> batch) {}

void FakeModelTypeChangeProcessor::OnSyncStarting(
    syncer::DataTypeErrorHandler* error_handler,
    const StartCallback& callback) {
  if (!callback.is_null()) {
    callback.Run(syncer::SyncError(), nullptr);
  }
}

void FakeModelTypeChangeProcessor::DisableSync() {}

syncer::SyncError FakeModelTypeChangeProcessor::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  return syncer::SyncError();
}

}  // namespace syncer_v2
