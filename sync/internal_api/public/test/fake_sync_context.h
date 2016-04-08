// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_CONTEXT_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_CONTEXT_H_

#include <memory>

#include "sync/internal_api/public/sync_context.h"

namespace syncer_v2 {

// A non-functional implementation of SyncContext for testing.
class FakeSyncContext : public SyncContext {
 public:
  FakeSyncContext();
  ~FakeSyncContext() override;

  void ConnectType(
      syncer::ModelType type,
      std::unique_ptr<ActivationContext> activation_context) override;
  void DisconnectType(syncer::ModelType type) override;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_CONTEXT_H_
