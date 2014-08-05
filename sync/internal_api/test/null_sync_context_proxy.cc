// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/null_sync_context_proxy.h"

namespace syncer {

NullSyncContextProxy::NullSyncContextProxy() {
}

NullSyncContextProxy::~NullSyncContextProxy() {
}

void NullSyncContextProxy::ConnectTypeToSync(
    syncer::ModelType type,
    const DataTypeState& data_type_state,
    const UpdateResponseDataList& saved_pending_updates,
    const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) {
  NOTREACHED() << "NullSyncContextProxy is not meant to be used";
}

void NullSyncContextProxy::Disconnect(syncer::ModelType type) {
  NOTREACHED() << "NullSyncContextProxy is not meant to be used";
}

scoped_ptr<SyncContextProxy> NullSyncContextProxy::Clone() const {
  return scoped_ptr<SyncContextProxy>(new NullSyncContextProxy());
}

}  // namespace syncer
