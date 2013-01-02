// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_APPLY_UPDATES_AND_RESOLVE_CONFLICTS_COMMAND_H_
#define SYNC_ENGINE_APPLY_UPDATES_AND_RESOLVE_CONFLICTS_COMMAND_H_

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/engine/model_changing_syncer_command.h"

namespace syncer {

class SYNC_EXPORT_PRIVATE ApplyUpdatesAndResolveConflictsCommand
    : public ModelChangingSyncerCommand {
 public:
  ApplyUpdatesAndResolveConflictsCommand();
  virtual ~ApplyUpdatesAndResolveConflictsCommand();

 protected:
  // ModelChangingSyncerCommand implementation.
  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE;
  virtual SyncerError ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplyUpdatesAndResolveConflictsCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_APPLY_UPDATES_AND_RESOLVE_CONFLICTS_COMMAND_H_
