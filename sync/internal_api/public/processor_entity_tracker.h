// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_PROCESSOR_ENTITY_TRACKER_H_
#define SYNC_INTERNAL_API_PUBLIC_PROCESSOR_ENTITY_TRACKER_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/entity_metadata.pb.h"

namespace syncer_v2 {
struct CommitRequestData;
struct UpdateResponseData;

// This class is used by the SharedModelTypeProcessor to track the state of each
// entity with its type. It can be considered a helper class internal to the
// processor. It manages the metadata for its entity and caches entity data
// upon a local change until commit confirmation is received.
class SYNC_EXPORT ProcessorEntityTracker {
 public:
  // Construct an instance representing a new locally-created item.
  static scoped_ptr<ProcessorEntityTracker> CreateNew(
      const std::string& client_tag,
      const std::string& client_tag_hash,
      const std::string& id,
      base::Time creation_time);

  // Construct an instance representing an item loaded from storage on init.
  // This method swaps out the contents of |metadata|.
  static scoped_ptr<ProcessorEntityTracker> CreateFromMetadata(
      const std::string& client_tag,
      sync_pb::EntityMetadata* metadata);

  ~ProcessorEntityTracker();

  // Returns entity's client tag.
  const std::string& client_tag() const { return client_tag_; }

  // Returns entity's metadata.
  const sync_pb::EntityMetadata& metadata() const { return metadata_; }

  // Returns true if this data is out of sync with the server.
  // A commit may or may not be in progress at this time.
  bool IsUnsynced() const;

  // Returns true if this data is out of sync with the sync thread.
  //
  // There may or may not be a commit in progress for this item, but there's
  // definitely no commit in progress for this (most up to date) version of
  // this item.
  bool RequiresCommitRequest() const;

  // Whether commit data is needed to be cached before a commit request can be
  // created. Note that deletions do not require cached data.
  bool RequiresCommitData() const;

  // Whether it's safe to clear the metadata for this entity. This means that
  // the entity is deleted and either knowledge of this entity has never left
  // this client or it is up to date with the server.
  bool CanClearMetadata() const;

  // Returns true if the specified update version does not contain new data.
  bool UpdateIsReflection(int64_t update_version) const;

  // Returns true if the specified update version conflicts with local changes.
  bool UpdateIsInConflict(int64_t update_version) const;

  // Applies an update from the sync server.
  //
  // Overrides any local changes.  Check UpdateIsInConflict() before calling
  // this function if you want to handle conflicts differently.
  void ApplyUpdateFromServer(const UpdateResponseData& response_data);

  // Applies a local change to this item.
  void MakeLocalChange(scoped_ptr<EntityData> data);

  // Schedule a commit if the |name| does not match this item's last known
  // encryption key.  The worker that performs the commit is expected to
  // encrypt the item using the latest available key.
  void UpdateDesiredEncryptionKey(const std::string& name);

  // Applies a local deletion to this item.
  void Delete();

  // Initializes a message representing this item's uncommitted state
  // and assumes that it is forwarded to the sync engine for commiting.
  void InitializeCommitRequestData(CommitRequestData* request);

  // Receives a successful commit response.
  //
  // Successful commit responses can overwrite an item's ID.
  //
  // Note that the receipt of a successful commit response does not necessarily
  // unset IsUnsynced().  If many local changes occur in quick succession, it's
  // possible that the committed item was already out of date by the time it
  // reached the server.
  void ReceiveCommitResponse(const std::string& id,
                             int64_t sequence_number,
                             int64_t response_version,
                             const std::string& encryption_key_name);

  // Clears any in-memory sync state associated with outstanding commits.
  void ClearTransientSyncState();

  // Takes the passed commit data and caches it in the instance.
  // The data is swapped from the input struct without copying.
  void CacheCommitData(EntityData* data);

  // Check if the instance has cached commit data.
  bool HasCommitData() const;

 private:
  friend class ProcessorEntityTrackerTest;

  // The constructor swaps the data from the passed metadata.
  ProcessorEntityTracker(const std::string& client_tag,
                         sync_pb::EntityMetadata* metadata);

  // Whether knowledge of this entity has never gone past the processor. This
  // means that no commits have been queued and it did not originate at the
  // server. This is used to determine whether it is safe to delete the tracker
  // and metadata for this entity.
  bool IsLocalOnly() const;

  // Increment sequence number in the metadata.
  void IncrementSequenceNumber();

  // Update hash string for EntitySpecifics in the metadata.
  void UpdateSpecificsHash(const sync_pb::EntitySpecifics& specifics);

  // Client tag. Should always be available.
  std::string client_tag_;

  // Serializable Sync metadata.
  sync_pb::EntityMetadata metadata_;

  // Sync data that exists for items being committed only.
  // The data is reset once commit confirmation is received.
  EntityDataPtr commit_data_;

  // The sequence number of the last item sent to the sync thread.
  int64_t commit_requested_sequence_number_;

  // TODO(stanisc): this should be removed.
  // The name of the encryption key used to encrypt this item on the server.
  // Empty when no encryption is in use.
  std::string encryption_key_name_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_PROCESSOR_ENTITY_TRACKER_H_
