// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_command.h"

namespace csync {

SyncerCommand::SyncerCommand() {}
SyncerCommand::~SyncerCommand() {}

SyncerError SyncerCommand::Execute(sessions::SyncSession* session) {
  SyncerError result = ExecuteImpl(session);
  return result;
}

}  // namespace csync
