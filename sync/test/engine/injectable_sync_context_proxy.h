// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_
#define SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer {

struct DataTypeState;
class ModelTypeSyncProxyImpl;
class ModelTypeSyncWorker;

// A SyncContextProxy implementation that, when a connection request is made,
// initalizes a connection to a previously injected ModelTypeSyncProxyImpl.
class InjectableSyncContextProxy : public syncer::SyncContextProxy {
 public:
  explicit InjectableSyncContextProxy(ModelTypeSyncWorker* worker);
  virtual ~InjectableSyncContextProxy();

  virtual void ConnectTypeToSync(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& type_sync_proxy)
      OVERRIDE;
  virtual void Disconnect(syncer::ModelType type) OVERRIDE;
  virtual scoped_ptr<SyncContextProxy> Clone() const OVERRIDE;

  ModelTypeSyncWorker* GetWorker();

 private:
  // A flag to ensure ConnectTypeToSync is called at most once.
  bool is_worker_connected_;

  // The ModelTypeSyncProxy's contract expects that it gets to own this object,
  // so we can retain only a non-owned pointer to it.
  //
  // This is very unsafe, but we can get away with it since these tests are not
  // exercising the proxy <-> worker connection code.
  ModelTypeSyncWorker* worker_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_
