// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_
#define SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/engine/model_changing_syncer_command.h"
#include "sync/engine/syncer_types.h"

namespace sync_pb {
class SyncEntity;
}

namespace syncer {

namespace syncable {
class WriteTransaction;
}

class Cryptographer;

// A syncer command for verifying and processing updates.
//
// Preconditions - Updates in the SyncerSesssion have been downloaded.
//
// Postconditions - All of the verified SyncEntity data will be copied to
//                  the server fields of the corresponding syncable entries.
class SYNC_EXPORT_PRIVATE ProcessUpdatesCommand
    : public ModelChangingSyncerCommand {
 public:
  ProcessUpdatesCommand();
  virtual ~ProcessUpdatesCommand();

 protected:
  // ModelChangingSyncerCommand implementation.
  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE;
  virtual SyncerError ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE;

 private:
  VerifyResult VerifyUpdate(
      syncable::WriteTransaction* trans,
      const sync_pb::SyncEntity& entry,
      ModelTypeSet requested_types,
      const ModelSafeRoutingInfo& routes);
  ServerUpdateProcessingResult ProcessUpdate(
      const sync_pb::SyncEntity& proto_update,
      const Cryptographer* cryptographer,
      syncable::WriteTransaction* const trans);
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_
