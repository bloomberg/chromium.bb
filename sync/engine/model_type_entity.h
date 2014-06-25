// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_ENTITY_H_
#define SYNC_ENGINE_MODEL_TYPE_ENTITY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/engine/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// This is the model thread's representation of a SyncEntity.
//
// The model type entity receives updates from the model itself and
// (asynchronously) from the sync server via the sync thread.  From the point
// of view of this class, updates from the server take precedence over local
// changes, though the model may be given an opportunity to overwrite this
// decision later.
//
// Sync will try to commit this entity's data to the sync server and local
// storage.
//
// Most of the logic related to those processes live outside this class.  This
// class helps out a bit by offering some functions to serialize its data to
// various formats and query the entity's status.
class SYNC_EXPORT_PRIVATE ModelTypeEntity {
 public:
  // Construct an instance representing a new locally-created item.
  static scoped_ptr<ModelTypeEntity> NewLocalItem(
      const std::string& client_tag,
      const sync_pb::EntitySpecifics& specifics,
      base::Time now);

  // Construct an instance representing an item newly received from the server.
  static scoped_ptr<ModelTypeEntity> FromServerUpdate(
      const std::string& id,
      const std::string& client_tag_hash,
      const std::string& non_unique_name,
      int64 version,
      const sync_pb::EntitySpecifics& specifics,
      bool deleted,
      base::Time ctime,
      base::Time mtime);

  // TODO(rlarocque): Implement FromDisk constructor when we implement storage.

  ~ModelTypeEntity();

  // Returns true if this data is out of sync with local storage.
  bool IsWriteRequired() const;

  // Returns true if this data is out of sync with the server.
  // A commit may or may not be in progress at this time.
  bool IsUnsynced() const;

  // Returns true if this data is out of sync with the sync thread.
  //
  // There may or may not be a commit in progress for this item, but there's
  // definitely no commit in progress for this (most up to date) version of
  // this item.
  bool RequiresCommitRequest() const;

  // Returns true if the specified update version does not contain new data.
  bool UpdateIsReflection(int64 update_version) const;

  // Returns true if the specified update version conflicts with local changes.
  bool UpdateIsInConflict(int64 update_version) const;

  // Applies an update from the sync server.
  //
  // Overrides any local changes.  Check UpdateIsInConflict() before calling
  // this function if you want to handle conflicts differently.
  void ApplyUpdateFromServer(int64 update_version,
                             bool deleted,
                             const sync_pb::EntitySpecifics& specifics,
                             base::Time mtime);

  // Applies a local change to this item.
  void MakeLocalChange(const sync_pb::EntitySpecifics& specifics);

  // Applies a local deletion to this item.
  void Delete();

  // Initializes a message representing this item's uncommitted state
  // to be forwarded to the sync server for committing.
  void InitializeCommitRequestData(CommitRequestData* request) const;

  // Notes that the current version of this item has been queued for commit.
  void SetCommitRequestInProgress();

  // Receives a successful commit response.
  //
  // Sucssful commit responses can overwrite an item's ID.
  //
  // Note that the receipt of a successful commit response does not necessarily
  // unset IsUnsynced().  If many local changes occur in quick succession, it's
  // possible that the committed item was already out of date by the time it
  // reached the server.
  void ReceiveCommitResponse(const std::string& id,
                             int64 sequence_number,
                             int64 response_version);

  // Clears any in-memory sync state associated with outstanding commits.
  void ClearTransientSyncState();

  // Clears all sync state.  Invoked when a user signs out.
  void ClearSyncState();

 private:
  ModelTypeEntity(int64 sequence_number,
                  int64 commit_requested_sequence_number,
                  int64 acked_sequence_number,
                  int64 base_version,
                  bool is_dirty,
                  const std::string& id,
                  const std::string& client_tag_hash,
                  const std::string& non_unique_name,
                  const sync_pb::EntitySpecifics& specifics,
                  bool deleted,
                  base::Time ctime,
                  base::Time mtime);

  // A sequence number used to track in-progress commits.  Each local change
  // increments this number.
  int64 sequence_number_;

  // The sequence number of the last item sent to the sync thread.
  int64 commit_requested_sequence_number_;

  // The sequence number of the last item known to be successfully committed.
  int64 acked_sequence_number_;

  // The server version on which this item is based.
  //
  // If there are no local changes, this is the version of the entity as we see
  // it here.
  //
  // If there are local changes, this is the version of the entity on which
  // those changes are based.
  int64 base_version_;

  // True if this entity is out of sync with local storage.
  bool is_dirty_;

  // The entity's ID.
  //
  // Most of the time, this is a server-assigned value.
  //
  // Prior to the item's first commit, we leave this value as an empty string.
  // The initial ID for a newly created item has to meet certain uniqueness
  // requirements, and we handle those on the sync thread.
  std::string id_;

  // A hash based on the client tag and model type.
  // Used for various map lookups.  Should always be available.
  std::string client_tag_hash_;

  // A non-unique name associated with this entity.
  //
  // It is sometimes used for debugging.  It gets saved to and restored from
  // the sync server.
  //
  // Its value is often related to the item's unhashed client tag, though this
  // is not guaranteed and should not be relied on.  May be hidden when
  // encryption is enabled.
  std::string non_unique_name_;

  // A protobuf filled with type-specific information.  Contains the most
  // up-to-date specifics, whether it be from the server or a locally modified
  // version.
  sync_pb::EntitySpecifics specifics_;

  // Whether or not the item is deleted.  The |specifics_| field may be empty
  // if this flag is true.
  bool deleted_;

  // Entity creation and modification timestamps.
  // Assigned by the client and synced by the server, though the server usually
  // doesn't bother to inspect their values.
  base::Time ctime_;
  base::Time mtime_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_MODEL_TYPE_ENTITY_H_
