// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CORE_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CORE_PROXY_H_

#include "sync/internal_api/public/sync_core_proxy.h"
#include "base/memory/weak_ptr.h"

namespace syncer {

class NonBlockingTypeProcessor;

// A non-functional implementation of SyncCoreProxy.
//
// It supports Clone(), but not much else.  Useful for testing.
class NullSyncCoreProxy : public SyncCoreProxy {
 public:
  NullSyncCoreProxy();
  virtual ~NullSyncCoreProxy();

  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      base::WeakPtr<NonBlockingTypeProcessor> processor) OVERRIDE;
  virtual scoped_ptr<SyncCoreProxy> Clone() OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CORE_PROXY_H_
