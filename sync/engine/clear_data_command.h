// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_CLEAR_DATA_COMMAND_H_
#define SYNC_ENGINE_CLEAR_DATA_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "sync/engine/syncer_command.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/syncable/model_type.h"

namespace browser_sync {

// Clears server data associated with this user's account
class ClearDataCommand : public SyncerCommand {
 public:
  ClearDataCommand();
  virtual ~ClearDataCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClearDataCommand);
};

}  // namespace browser_sync

#endif  // SYNC_ENGINE_CLEAR_DATA_COMMAND_H_
