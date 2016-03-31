// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/model_type_service.h"

#include "sync/api/model_type_change_processor.h"

namespace syncer_v2 {

ModelTypeService::ModelTypeService(
    const ChangeProcessorFactory& change_processor_factory,
    syncer::ModelType type)
    : change_processor_factory_(change_processor_factory), type_(type) {}

ModelTypeService::~ModelTypeService() {}

ModelTypeChangeProcessor* ModelTypeService::change_processor() const {
  return change_processor_.get();
}

ModelTypeChangeProcessor* ModelTypeService::GetOrCreateChangeProcessor() {
  if (!change_processor_) {
    change_processor_ = change_processor_factory_.Run(type(), this);
    DCHECK(change_processor_);
    OnChangeProcessorSet();
  }
  return change_processor_.get();
}

void ModelTypeService::clear_change_processor() {
  change_processor_.reset();
}

void ModelTypeService::OnSyncStarting(
    const ModelTypeChangeProcessor::StartCallback& start_callback) {
  GetOrCreateChangeProcessor()->OnSyncStarting(start_callback);
}

syncer::ModelType ModelTypeService::type() const {
  return type_;
}

}  // namespace syncer_v2
