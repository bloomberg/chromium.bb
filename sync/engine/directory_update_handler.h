// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DIRECTORY_UPDATE_HANDLER_H_
#define SYNC_ENGINE_DIRECTORY_UPDATE_HANDLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/engine/process_updates_util.h"
#include "sync/engine/update_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace sync_pb {
class DataTypeProgressMarker;
class GarbageCollectionDirective;
class GetUpdatesResponse;
}

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class Directory;
}

class DirectoryTypeDebugInfoEmitter;
class ModelSafeWorker;

// This class represents the syncable::Directory's processes for requesting and
// processing updates from the sync server.
//
// Each instance of this class represents a particular type in the
// syncable::Directory.  It can store and retreive that type's progress markers.
// It can also process a set of received SyncEntities and store their data.
class SYNC_EXPORT_PRIVATE DirectoryUpdateHandler : public UpdateHandler {
 public:
  DirectoryUpdateHandler(syncable::Directory* dir,
                         ModelType type,
                         scoped_refptr<ModelSafeWorker> worker,
                         DirectoryTypeDebugInfoEmitter* debug_info_emitter);
  virtual ~DirectoryUpdateHandler();

  // UpdateHandler implementation.
  virtual void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const OVERRIDE;
  virtual void GetDataTypeContext(sync_pb::DataTypeContext* context) const
      OVERRIDE;
  virtual SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) OVERRIDE;
  virtual void ApplyUpdates(sessions::StatusController* status) OVERRIDE;
  virtual void PassiveApplyUpdates(sessions::StatusController* status) OVERRIDE;

 private:
  friend class DirectoryUpdateHandlerApplyUpdateTest;
  friend class DirectoryUpdateHandlerProcessUpdateTest;

  // Sometimes there is nothing to do, so we can return without doing anything.
  bool IsApplyUpdatesRequired();

  // Processes the given SyncEntities and stores their data in the directory.
  // Their types must match this update handler's type.
  void UpdateSyncEntities(
      syncable::ModelNeutralWriteTransaction* trans,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status);

  // Expires entries according to GC directives.
  void ExpireEntriesIfNeeded(
      syncable::ModelNeutralWriteTransaction* trans,
      const sync_pb::DataTypeProgressMarker& progress_marker);

  // Stores the given progress marker in the directory.
  // Its type must match this update handler's type.
  void UpdateProgressMarker(
      const sync_pb::DataTypeProgressMarker& progress_marker);

  bool IsValidProgressMarker(
      const sync_pb::DataTypeProgressMarker& progress_marker) const;

  // Skips all checks and goes straight to applying the updates.
  SyncerError ApplyUpdatesImpl(sessions::StatusController* status);

  syncable::Directory* dir_;
  ModelType type_;
  scoped_refptr<ModelSafeWorker> worker_;
  DirectoryTypeDebugInfoEmitter* debug_info_emitter_;

  scoped_ptr<sync_pb::GarbageCollectionDirective> cached_gc_directive_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryUpdateHandler);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_DIRECTORY_UPDATE_HANDLER_H_
