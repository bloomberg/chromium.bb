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
class GetUpdatesResponse;
class SyncEntity;
}

namespace syncer {

namespace syncable {
class ModelNeutralWriteTransaction;
}

class Cryptographer;

// A syncer command for verifying and processing updates.
//
// Preconditions - Updates in the SyncerSesssion have been downloaded.
//
// Postconditions - All of the verified SyncEntity data will be copied to
//                  the server fields of the corresponding syncable entries.
class SYNC_EXPORT_PRIVATE ProcessUpdatesCommand : public SyncerCommand {
 public:
  ProcessUpdatesCommand();
  virtual ~ProcessUpdatesCommand();

 protected:
  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;
  typedef std::map<ModelType, SyncEntityList> TypeSyncEntityMap;

  // Given a GetUpdates response, iterates over all the returned items and
  // divides them according to their type.  Outputs a map from model types to
  // received SyncEntities.  ModelTypes that had no items in the update response
  // will not be included in this map.
  static void PartitionUpdatesByType(
      const sync_pb::GetUpdatesResponse& updates,
      TypeSyncEntityMap* updates_by_type);

  VerifyResult VerifyUpdate(
      syncable::ModelNeutralWriteTransaction* trans,
      const sync_pb::SyncEntity& entry,
      ModelTypeSet requested_types,
      const ModelSafeRoutingInfo& routes);
  void ProcessUpdate(
      const sync_pb::SyncEntity& proto_update,
      const Cryptographer* cryptographer,
      syncable::ModelNeutralWriteTransaction* const trans);
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_
