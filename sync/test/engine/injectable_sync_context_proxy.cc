// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/injectable_sync_context_proxy.h"

#include "sync/engine/commit_queue.h"
#include "sync/engine/model_type_processor_impl.h"
#include "sync/internal_api/public/activation_context.h"

namespace syncer_v2 {

InjectableSyncContextProxy::InjectableSyncContextProxy(
    CommitQueue* queue)
    : is_worker_connected_(false), queue_(queue) {
}

InjectableSyncContextProxy::~InjectableSyncContextProxy() {
}

void InjectableSyncContextProxy::ConnectTypeToSync(
    syncer::ModelType type,
    scoped_ptr<ActivationContext> activation_context) {
  // This class is allowed to participate in only one connection.
  DCHECK(!is_worker_connected_);
  is_worker_connected_ = true;

  // Hands off ownership of our member to the type_processor, while keeping
  // an unsafe pointer to it.  This is why we can only connect once.
  scoped_ptr<CommitQueue> queue(queue_);

  activation_context->type_processor->OnConnect(queue.Pass());
}

void InjectableSyncContextProxy::Disconnect(syncer::ModelType type) {
  // This should delete the queue, but we don't own it.
  queue_ = NULL;
}

scoped_ptr<SyncContextProxy> InjectableSyncContextProxy::Clone() const {
  // This confuses ownership.  We trust that our callers are well-behaved.
  return scoped_ptr<SyncContextProxy>(new InjectableSyncContextProxy(queue_));
}

CommitQueue* InjectableSyncContextProxy::GetQueue() {
  return queue_;
}

}  // namespace syncer
