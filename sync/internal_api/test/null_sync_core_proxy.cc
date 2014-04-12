// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/null_sync_core_proxy.h"

namespace syncer {

NullSyncCoreProxy::NullSyncCoreProxy() {}

NullSyncCoreProxy::~NullSyncCoreProxy() {}

void NullSyncCoreProxy::ConnectTypeToCore(
    syncer::ModelType type,
      base::WeakPtr<NonBlockingTypeProcessor> processor) {
  NOTREACHED() << "NullSyncCoreProxy is not meant to be used";
}

scoped_ptr<SyncCoreProxy> NullSyncCoreProxy::Clone() {
  return scoped_ptr<SyncCoreProxy>(new NullSyncCoreProxy());
}

}  // namespace syncer
