// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_WORKER_ENTITY_TRACKER_H_
#define SYNC_ENGINE_WORKER_ENTITY_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {
struct CommitRequestData;
struct CommitResponseData;
struct UpdateResponseData;

// Manages the pending commit and update state for an entity on the sync
// thread.
//
// It should be considered a helper class internal to the
// ModelTypeWorker.
//
// Maintains the state associated with a particular sync entity which is
// necessary for decision-making on the sync thread.  It can track pending
// commit state, received update state, and can detect conflicts.
//
// This object may contain state associated with a pending commit, pending
// update, or both.
class SYNC_EXPORT WorkerEntityTracker {
 public:
  // Initializes the entity tracker's main fields. Does not initialize state
  // related to a pending commit.
  WorkerEntityTracker(const std::string& id,
                      const std::string& client_tag_hash);

  ~WorkerEntityTracker();

  // Returns true if this entity should be commited to the server.
  bool HasPendingCommit() const;

  // Populates a sync_pb::SyncEntity for a commit.
  void PopulateCommitProto(sync_pb::SyncEntity* commit_entity) const;

  // Updates this entity with data from the latest version that the
  // model asked us to commit.  May clobber state related to the
  // model's previous commit attempt(s).
  void RequestCommit(const CommitRequestData& data);

  // Tracks the receipt of a commit response and fills in some local-only data
  // on it to be passed back to the processor.
  void ReceiveCommitResponse(CommitResponseData* ack);

  // Handles receipt of an update from the server.
  void ReceiveUpdate(int64_t version);

  // Handles the receipt of an encrypted update from the server.
  //
  // Returns true if the tracker decides this item is worth keeping.  Returns
  // false if the item is discarded, which could happen if the version number
  // is out of date.
  bool ReceiveEncryptedUpdate(const UpdateResponseData& data);

  // Functions to fetch the latest encrypted update.
  bool HasEncryptedUpdate() const;
  UpdateResponseData GetEncryptedUpdate() const;

  // Clears the encrypted update. Allows us to resume regular commit behavior.
  void ClearEncryptedUpdate();

 private:
  // Checks if the current state indicates a conflict.
  //
  // This can be true only while a call to this object is in progress.
  // Conflicts are always cleared before the method call ends.
  bool IsInConflict() const;

  // Checks if the server knows about this item.
  bool IsServerKnown() const;

  // Clears flag and optionally clears state associated with a pending commit.
  void ClearPendingCommit();

  // The ID for this entry.  May be empty if the entry has never been committed.
  std::string id_;

  // The hashed client tag for this entry.
  std::string client_tag_hash_;

  // The highest version seen in a commit response for this entry.
  int64_t highest_commit_response_version_;

  // The highest version seen in a GU response for this entry.
  int64_t highest_gu_response_version_;

  // Used to track in-flight commit requests on the model thread.  All we need
  // to do here is return it back to the model thread when the pending commit
  // is completed and confirmed.  Not valid if no commit is pending.
  int64_t sequence_number_;

  // The server version on which this item is based.
  int64_t base_version_;

  // A commit for this entity waiting for a sync cycle to be committed.
  std::unique_ptr<CommitRequestData> pending_commit_;

  // The specifics hash for the pending commit if there is one, "" otherwise.
  std::string pending_commit_specifics_hash_;

  // An update for this entity which can't be applied right now. The presence
  // of an pending update prevents commits.  As of this writing, the only
  // source of pending updates is updates that can't currently be decrypted.
  std::unique_ptr<UpdateResponseData> encrypted_update_;

  DISALLOW_COPY_AND_ASSIGN(WorkerEntityTracker);
};

}  // namespace syncer_v2

#endif  // SYNC_ENGINE_WORKER_ENTITY_TRACKER_H_
