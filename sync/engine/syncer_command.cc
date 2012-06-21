// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_command.h"

#include "sync/engine/net/server_connection_manager.h"
#include "sync/sessions/sync_session.h"

namespace csync {
using sessions::SyncSession;

SyncerCommand::SyncerCommand() {}
SyncerCommand::~SyncerCommand() {}

SyncerError SyncerCommand::Execute(SyncSession* session) {
  SyncerError result = ExecuteImpl(session);
  SendNotifications(session);
  return result;
}

void SyncerCommand::SendNotifications(SyncSession* session) {
  if (session->mutable_status_controller()->TestAndClearIsDirty()) {
    SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
    event.snapshot = session->TakeSnapshot();
    session->context()->NotifyListeners(event);
  }
}

}  // namespace csync
