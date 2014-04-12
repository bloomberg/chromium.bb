// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class NonBlockingTypeProcessor;

// Interface for the datatype integration logic from non-sync threads.
//
// See SyncCoreProxyImpl for an actual implementation.
class SYNC_EXPORT_PRIVATE SyncCoreProxy {
 public:
  SyncCoreProxy();
  virtual ~SyncCoreProxy();

  // Attempts to connect a non-blocking type to the sync core.
  //
  // Must be called from the thread where the data type lives.
  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      base::WeakPtr<NonBlockingTypeProcessor> type_processor) = 0;

  // Creates a clone of this SyncCoreProxy.
  virtual scoped_ptr<SyncCoreProxy> Clone() = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_
