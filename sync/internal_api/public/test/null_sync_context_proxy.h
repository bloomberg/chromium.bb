// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer {

class ModelTypeSyncProxyImpl;

// A non-functional implementation of SyncContextProxy.
//
// It supports Clone(), but not much else.  Useful for testing.
class NullSyncContextProxy : public SyncContextProxy {
 public:
  NullSyncContextProxy();
  virtual ~NullSyncContextProxy();

  virtual void ConnectTypeToSync(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const UpdateResponseDataList& saved_pending_updates,
      const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) OVERRIDE;
  virtual void Disconnect(syncer::ModelType type) OVERRIDE;
  virtual scoped_ptr<SyncContextProxy> Clone() const OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_
