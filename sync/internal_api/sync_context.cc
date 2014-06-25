// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_context.h"

#include "sync/engine/model_type_sync_worker_impl.h"
#include "sync/sessions/model_type_registry.h"

namespace syncer {

SyncContext::SyncContext(ModelTypeRegistry* model_type_registry)
    : model_type_registry_(model_type_registry), weak_ptr_factory_(this) {
}

SyncContext::~SyncContext() {
}

void SyncContext::ConnectSyncTypeToWorker(
    ModelType type,
    const DataTypeState& data_type_state,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) {
  // Initialize the type sync proxy's sync-thread sibling and the
  // ModelTypeSyncProxy <-> ModelTypeSyncWorker
  // (ie. model thread <-> sync thread) communication channel.
  model_type_registry_->InitializeNonBlockingType(
      type, data_type_state, task_runner, type_sync_proxy);
}

void SyncContext::Disconnect(ModelType type) {
  model_type_registry_->RemoveNonBlockingType(type);
}

base::WeakPtr<SyncContext> SyncContext::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
