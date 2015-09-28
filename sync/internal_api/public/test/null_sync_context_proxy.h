// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_

#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer_v2 {

// A non-functional implementation of SyncContextProxy.
//
// It supports Clone(), but not much else.  Useful for testing.
class NullSyncContextProxy : public SyncContextProxy {
 public:
  NullSyncContextProxy();
  ~NullSyncContextProxy() override;

  void ConnectTypeToSync(
      syncer::ModelType type,
      scoped_ptr<ActivationContext> activation_context) override;
  void Disconnect(syncer::ModelType type) override;
  scoped_ptr<SyncContextProxy> Clone() const override;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_NULL_SYNC_CONTEXT_PROXY_H_
