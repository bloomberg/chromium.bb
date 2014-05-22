// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_core.h"

#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/sessions/model_type_registry.h"

namespace syncer {

SyncCore::SyncCore(ModelTypeRegistry* model_type_registry)
    : model_type_registry_(model_type_registry), weak_ptr_factory_(this) {}

SyncCore::~SyncCore() {}

void SyncCore::ConnectSyncTypeToCore(
    ModelType type,
    const DataTypeState& data_type_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<NonBlockingTypeProcessor> processor) {
  // Initialize the processor's sync-thread sibling and the
  // processor <-> processor_core (ie. model thread <-> sync thread)
  // communication channel.
  model_type_registry_->InitializeNonBlockingType(
      type, data_type_state, task_runner, processor);
}

void SyncCore::Disconnect(ModelType type) {
  model_type_registry_->RemoveNonBlockingType(type);
}

base::WeakPtr<SyncCore> SyncCore::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
