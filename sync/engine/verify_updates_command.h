// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_
#define SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/engine/model_changing_syncer_command.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/syncproto.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace syncable {
class WriteTransaction;
}

namespace browser_sync {

// Verifies the response from a GetUpdates request. All invalid updates will be
// noted in the SyncSession after this command is executed.
class VerifyUpdatesCommand : public ModelChangingSyncerCommand {
 public:
  VerifyUpdatesCommand();
  virtual ~VerifyUpdatesCommand();

 protected:
  // ModelChangingSyncerCommand implementation.
  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE;
  virtual SyncerError ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE;

 private:
  struct VerifyUpdateResult {
    VerifyResult value;
    ModelSafeGroup placement;
  };
  VerifyUpdateResult VerifyUpdate(syncable::WriteTransaction* trans,
                                  const SyncEntity& entry,
                                  const syncable::ModelTypeSet& requested_types,
                                  const ModelSafeRoutingInfo& routes);
  DISALLOW_COPY_AND_ASSIGN(VerifyUpdatesCommand);
};

}  // namespace browser_sync

#endif  // SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_
