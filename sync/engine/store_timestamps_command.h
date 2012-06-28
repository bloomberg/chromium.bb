// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_
#define SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_
#pragma once

#include "base/compiler_specific.h"
#include "sync/engine/syncer_command.h"
#include "sync/engine/syncer_types.h"

namespace syncer {

// A syncer command that extracts the changelog timestamp information from
// a GetUpdatesResponse (fetched in DownloadUpdatesCommand) and stores
// it in the directory.  This is meant to run immediately after
// ProcessUpdatesCommand.
//
// Preconditions - all updates in the SyncerSesssion have been stored in the
//                 database, meaning it is safe to update the persisted
//                 timestamps.
//
// Postconditions - The next_timestamp returned by the server will be
//                  saved into the directory (where it will be used
//                  the next time that DownloadUpdatesCommand runs).
class StoreTimestampsCommand : public SyncerCommand {
 public:
  StoreTimestampsCommand();
  virtual ~StoreTimestampsCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreTimestampsCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_
