// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sync_core_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "sync/internal_api/sync_core.h"

namespace syncer {

SyncCoreProxy::SyncCoreProxy(
    scoped_refptr<base::SequencedTaskRunner> sync_task_runner,
    base::WeakPtr<SyncCore> sync_core)
    : sync_task_runner_(sync_task_runner),
      sync_core_(sync_core) {}

SyncCoreProxy::~SyncCoreProxy() {}

void SyncCoreProxy::ConnectTypeToCore(
    ModelType type,
    base::WeakPtr<NonBlockingTypeProcessor> type_processor) {
  VLOG(1) << "ConnectSyncTypeToCore: " << ModelTypeToString(type);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncCore::ConnectSyncTypeToCore,
                 sync_core_,
                 type,
                 base::MessageLoopProxy::current(),
                 type_processor));
}

SyncCoreProxy SyncCoreProxy::GetInvalidSyncCoreProxyForTest() {
  return SyncCoreProxy(scoped_refptr<base::SequencedTaskRunner>(),
                       base::WeakPtr<SyncCore>());
}

}  // namespace syncer
