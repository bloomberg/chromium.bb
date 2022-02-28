// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/sync_engine_host_stub.h"

namespace syncer {

SyncEngineHostStub::SyncEngineHostStub() = default;
SyncEngineHostStub::~SyncEngineHostStub() = default;

void SyncEngineHostStub::OnEngineInitialized(
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    bool success,
    bool is_first_time_sync_configure) {}

void SyncEngineHostStub::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {}

void SyncEngineHostStub::OnProtocolEvent(const ProtocolEvent& event) {}

void SyncEngineHostStub::OnConnectionStatusChange(ConnectionStatus status) {}

void SyncEngineHostStub::OnMigrationNeededForTypes(ModelTypeSet types) {}

void SyncEngineHostStub::OnActionableError(const SyncProtocolError& error) {}

void SyncEngineHostStub::OnBackedOffTypesChanged() {}

}  // namespace syncer
