// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/model_type_service.h"

#include "sync/api/model_type_change_processor.h"
#include "sync/internal_api/public/data_type_error_handler.h"

namespace syncer_v2 {

ModelTypeService::ModelTypeService(
    const ChangeProcessorFactory& change_processor_factory,
    syncer::ModelType type)
    : change_processor_factory_(change_processor_factory), type_(type) {}

ModelTypeService::~ModelTypeService() {}

ConflictResolution ModelTypeService::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) const {
  // TODO(maxbogue): Add tests once a file exists for them (crbug.com/543407).
  if (remote_data.is_deleted()) {
    DCHECK(!local_data.is_deleted());
    return ConflictResolution::UseLocal();
  }
  return ConflictResolution::UseRemote();
}

void ModelTypeService::OnSyncStarting(
    syncer::DataTypeErrorHandler* error_handler,
    const ModelTypeChangeProcessor::StartCallback& start_callback) {
  CreateChangeProcessor();
  change_processor_->OnSyncStarting(error_handler, start_callback);
}

void ModelTypeService::DisableSync() {
  DCHECK(change_processor_);
  change_processor_->DisableSync();
  change_processor_.reset();
}

void ModelTypeService::CreateChangeProcessor() {
  if (!change_processor_) {
    change_processor_ = change_processor_factory_.Run(type_, this);
    DCHECK(change_processor_);
    OnChangeProcessorSet();
  }
}

ModelTypeChangeProcessor* ModelTypeService::change_processor() const {
  return change_processor_.get();
}

void ModelTypeService::clear_change_processor() {
  change_processor_.reset();
}

}  // namespace syncer_v2
