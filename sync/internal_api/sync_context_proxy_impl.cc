// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_context_proxy_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/sync_context.h"

namespace syncer {

SyncContextProxyImpl::SyncContextProxyImpl(
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner,
    const base::WeakPtr<SyncContext>& sync_context)
    : sync_task_runner_(sync_task_runner), sync_context_(sync_context) {
}

SyncContextProxyImpl::~SyncContextProxyImpl() {
}

void SyncContextProxyImpl::ConnectTypeToSync(
    ModelType type,
    const DataTypeState& data_type_state,
    const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) {
  VLOG(1) << "ConnectTypeToSync: " << ModelTypeToString(type);
  sync_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&SyncContext::ConnectSyncTypeToWorker,
                                         sync_context_,
                                         type,
                                         data_type_state,
                                         base::ThreadTaskRunnerHandle::Get(),
                                         type_sync_proxy));
}

void SyncContextProxyImpl::Disconnect(ModelType type) {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncContext::DisconnectSyncWorker, sync_context_, type));
}

scoped_ptr<SyncContextProxy> SyncContextProxyImpl::Clone() const {
  return scoped_ptr<SyncContextProxy>(
      new SyncContextProxyImpl(sync_task_runner_, sync_context_));
}

}  // namespace syncer
