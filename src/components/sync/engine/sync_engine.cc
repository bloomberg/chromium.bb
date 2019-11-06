// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_engine.h"

namespace syncer {

SyncEngine::InitParams::InitParams() = default;
SyncEngine::InitParams::InitParams(InitParams&& other) = default;
SyncEngine::InitParams::~InitParams() = default;

SyncEngine::SyncEngine() = default;
SyncEngine::~SyncEngine() = default;

}  // namespace syncer
