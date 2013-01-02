// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNCER_COMMAND_H_
#define SYNC_ENGINE_SYNCER_COMMAND_H_

#include "base/basictypes.h"

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace syncer {

namespace sessions {
class SyncSession;
}

// Implementation of a simple command pattern intended to be driven by the
// Syncer. SyncerCommand is abstract and all subclasses must implement
// ExecuteImpl(). This is done so that chunks of syncer operation can be unit
// tested.
//
// Example Usage:
//
//   SyncSession session = ...;
//   SyncerCommand *cmd = SomeCommandFactory.createCommand(...);
//   cmd->Execute(session);
//   delete cmd;

class SYNC_EXPORT_PRIVATE SyncerCommand {
 public:
  SyncerCommand();
  virtual ~SyncerCommand();

  // Execute dispatches to a derived class's ExecuteImpl.
  SyncerError Execute(sessions::SyncSession* session);

  // ExecuteImpl is where derived classes actually do work.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncerCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNCER_COMMAND_H_
