// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/injectable_sync_context_proxy.h"

#include "sync/engine/model_type_sync_proxy_impl.h"
#include "sync/engine/model_type_sync_worker.h"

namespace syncer {

InjectableSyncContextProxy::InjectableSyncContextProxy(
    ModelTypeSyncWorker* worker)
    : is_worker_connected_(false), worker_(worker) {
}

InjectableSyncContextProxy::~InjectableSyncContextProxy() {
}

void InjectableSyncContextProxy::ConnectTypeToSync(
    syncer::ModelType type,
    const DataTypeState& data_type_state,
    const UpdateResponseDataList& response_list,
    const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& type_sync_proxy) {
  // This class is allowed to participate in only one connection.
  DCHECK(!is_worker_connected_);
  is_worker_connected_ = true;

  // Hands off ownership of our member to the type_sync_proxy, while keeping
  // an unsafe pointer to it.  This is why we can only connect once.
  scoped_ptr<ModelTypeSyncWorker> worker(worker_);

  type_sync_proxy->OnConnect(worker.Pass());
}

void InjectableSyncContextProxy::Disconnect(syncer::ModelType type) {
  // This should delete the worker, but we don't own it.
  worker_ = NULL;
}

scoped_ptr<SyncContextProxy> InjectableSyncContextProxy::Clone() const {
  // This confuses ownership.  We trust that our callers are well-behaved.
  return scoped_ptr<SyncContextProxy>(new InjectableSyncContextProxy(worker_));
}

ModelTypeSyncWorker* InjectableSyncContextProxy::GetWorker() {
  return worker_;
}

}  // namespace syncer
