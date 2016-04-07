// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_context_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/sync_context.h"

namespace syncer_v2 {

SyncContextProxy::SyncContextProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<SyncContext>& sync_context)
    : task_runner_(task_runner), sync_context_(sync_context) {}

SyncContextProxy::~SyncContextProxy() {}

void SyncContextProxy::ConnectType(
    syncer::ModelType type,
    scoped_ptr<ActivationContext> activation_context) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncContext::ConnectType, sync_context_, type,
                            base::Passed(&activation_context)));
}

void SyncContextProxy::DisconnectType(syncer::ModelType type) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncContext::DisconnectType, sync_context_, type));
}

}  // namespace syncer_v2
