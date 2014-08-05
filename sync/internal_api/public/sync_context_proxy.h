// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class ModelTypeSyncProxyImpl;
struct DataTypeState;

// Interface for the datatype integration logic from non-sync threads.
//
// See SyncContextProxyImpl for an actual implementation.
class SYNC_EXPORT_PRIVATE SyncContextProxy {
 public:
  SyncContextProxy();
  virtual ~SyncContextProxy();

  // Attempts to connect a non-blocking type to the sync context.
  //
  // Must be called from the thread where the data type lives.
  virtual void ConnectTypeToSync(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) = 0;

  // Tells the syncer that we're no longer interested in syncing this type.
  //
  // Once this takes effect, the syncer can assume that it will no longer
  // receive commit requests for this type.  It should also stop requesting
  // and applying updates for this type, too.
  virtual void Disconnect(syncer::ModelType type) = 0;

  // Creates a clone of this SyncContextProxy.
  virtual scoped_ptr<SyncContextProxy> Clone() const = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_
