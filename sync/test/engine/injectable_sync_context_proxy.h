// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_
#define SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer_v2 {

class ModelTypeProcessor;
class CommitQueue;

// A SyncContextProxy implementation that, when a connection request is made,
// initalizes a connection to a previously injected ModelTypeProcessor.
class InjectableSyncContextProxy : public SyncContextProxy {
 public:
  explicit InjectableSyncContextProxy(CommitQueue* queue);
  ~InjectableSyncContextProxy() override;

  void ConnectTypeToSync(
      syncer::ModelType type,
      scoped_ptr<ActivationContext> activation_context) override;
  void Disconnect(syncer::ModelType type) override;
  scoped_ptr<SyncContextProxy> Clone() const override;

  CommitQueue* GetQueue();

 private:
  // A flag to ensure ConnectTypeToSync is called at most once.
  bool is_worker_connected_;

  // The ModelTypeProcessor's contract expects that it gets to own this object,
  // so we can retain only a non-owned pointer to it.
  //
  // This is very unsafe, but we can get away with it since these tests are not
  // exercising the proxy <-> worker connection code.
  CommitQueue* queue_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_INJECTABLE_SYNC_CONTEXT_PROXY_H_
