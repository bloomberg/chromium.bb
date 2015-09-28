// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_context_proxy_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/sync_context.h"

namespace syncer_v2 {

SyncContextProxyImpl::SyncContextProxyImpl(
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner,
    const base::WeakPtr<SyncContext>& sync_context)
    : sync_task_runner_(sync_task_runner), sync_context_(sync_context) {
}

SyncContextProxyImpl::~SyncContextProxyImpl() {
}

void SyncContextProxyImpl::ConnectTypeToSync(
    syncer::ModelType type,
    scoped_ptr<ActivationContext> activation_context) {
  VLOG(1) << "ConnectTypeToSync: " << ModelTypeToString(type);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncContext::ConnectSyncTypeToWorker, sync_context_, type,
                 base::Passed(&activation_context)));
}

void SyncContextProxyImpl::Disconnect(syncer::ModelType type) {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncContext::DisconnectSyncWorker, sync_context_, type));
}

scoped_ptr<SyncContextProxy> SyncContextProxyImpl::Clone() const {
  return scoped_ptr<SyncContextProxy>(
      new SyncContextProxyImpl(sync_task_runner_, sync_context_));
}

}  // namespace syncer
