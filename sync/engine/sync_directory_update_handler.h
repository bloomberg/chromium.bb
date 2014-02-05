// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_DIRECTORY_UPDATE_HANDLER_H_
#define SYNC_ENGINE_SYNC_DIRECTORY_UPDATE_HANDLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "sync/base/sync_export.h"
#include "sync/engine/process_updates_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace sync_pb {
class DataTypeProgressMarker;
class GetUpdatesResponse;
}

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class Directory;
}

class ModelSafeWorker;

// This class represents the syncable::Directory's processes for requesting and
// processing updates from the sync server.
//
// Each instance of this class represents a particular type in the
// syncable::Directory.  It can store and retreive that type's progress markers.
// It can also process a set of received SyncEntities and store their data.
class SYNC_EXPORT_PRIVATE SyncDirectoryUpdateHandler {
 public:
  SyncDirectoryUpdateHandler(syncable::Directory* dir,
                             ModelType type,
                             scoped_refptr<ModelSafeWorker> worker);
  ~SyncDirectoryUpdateHandler();

  // Fills the given parameter with the stored progress marker for this type.
  void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const;

  // Processes the contents of a GetUpdates response message.
  //
  // Should be invoked with the progress marker and set of SyncEntities from a
  // single GetUpdates response message.  The progress marker's type must match
  // this update handler's type, and the set of SyncEntities must include all
  // entities of this type found in the response message.
  void ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status);

  // If there are updates to apply, apply them on the proper thread.
  // Delegates to ApplyUpdatesImpl().
  void ApplyUpdates(sessions::StatusController* status);

  // Apply updates on the sync thread.  This is for use during initial sync
  // prior to model association.
  void PassiveApplyUpdates(sessions::StatusController* status);

 private:
  friend class SyncDirectoryUpdateHandlerApplyUpdateTest;
  friend class SyncDirectoryUpdateHandlerProcessUpdateTest;

  // Sometimes there is nothing to do, so we can return without doing anything.
  bool IsApplyUpdatesRequired();

  // Processes the given SyncEntities and stores their data in the directory.
  // Their types must match this update handler's type.
  void UpdateSyncEntities(
      syncable::ModelNeutralWriteTransaction* trans,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status);

  // Stores the given progress marker in the directory.
  // Its type must match this update handler's type.
  void UpdateProgressMarker(
      const sync_pb::DataTypeProgressMarker& progress_marker);

  // Skips all checks and goes straight to applying the updates.
  SyncerError ApplyUpdatesImpl(sessions::StatusController* status);

  syncable::Directory* dir_;
  ModelType type_;
  scoped_refptr<ModelSafeWorker> worker_;

  DISALLOW_COPY_AND_ASSIGN(SyncDirectoryUpdateHandler);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_DIRECTORY_UPDATE_HANDLER_H_
