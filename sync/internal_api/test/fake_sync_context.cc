// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_sync_context.h"

#include "sync/internal_api/public/activation_context.h"

namespace syncer_v2 {

FakeSyncContext::FakeSyncContext() {}

FakeSyncContext::~FakeSyncContext() {}

void FakeSyncContext::ConnectType(
    syncer::ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {
  NOTREACHED() << "FakeSyncContext is not meant to be used";
}

void FakeSyncContext::DisconnectType(syncer::ModelType type) {
  NOTREACHED() << "FakeSyncContext is not meant to be used";
}

}  // namespace syncer_v2
