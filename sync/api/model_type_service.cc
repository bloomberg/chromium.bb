// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/model_type_service.h"

namespace syncer_v2 {

ModelTypeService::ModelTypeService() {}

ModelTypeService::~ModelTypeService() {}

ModelTypeChangeProcessor* ModelTypeService::change_processor() const {
  return change_processor_.get();
}

void ModelTypeService::set_change_processor(
    scoped_ptr<ModelTypeChangeProcessor> change_processor) {
  DCHECK(!change_processor_);
  change_processor_.swap(change_processor);
  OnChangeProcessorSet();
}

void ModelTypeService::clear_change_processor() {
  change_processor_.reset();
}

}  // namespace syncer_v2
