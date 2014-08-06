// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_ENTITY_TRACKER_H_
#define SYNC_ENGINE_ENTITY_TRACKER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// Manages the pending commit and update state for an entity on the sync
// thread.
//
// It should be considered a helper class internal to the
// ModelTypeSyncWorker.
//
// Maintains the state associated with a particular sync entity which is
// necessary for decision-making on the sync thread.  It can track pending
// commit state, received update state, and can detect conflicts.
//
// This object may or may not contain state associated with a pending commit.
// If no commit is pending, the |is_commit_pending_| flag will be set to false
// and many of this object's fields will be cleared.
class SYNC_EXPORT EntityTracker {
 public:
  ~EntityTracker();

  // Initialize a new entity based on an update response.
  static EntityTracker* FromServerUpdate(const std::string& id_string,
                                         const std::string& client_tag_hash,
                                         int64 version);

  // Initialize a new entity based on a commit request.
  static EntityTracker* FromCommitRequest(
      const std::string& id_string,
      const std::string& client_tag_hash,
      int64 sequence_number,
      int64 base_version,
      base::Time ctime,
      base::Time mtime,
      const std::string& non_unique_name,
      bool deleted,
      const sync_pb::EntitySpecifics& specifics);

  // Returns true if this entity should be commited to the server.
  bool IsCommitPending() const;

  // Populates a sync_pb::SyncEntity for a commit.  Also sets the
  // |sequence_number|, so we can track it throughout the commit process.
  void PrepareCommitProto(sync_pb::SyncEntity* commit_entity,
                          int64* sequence_number) const;

  // Updates this entity with data from the latest version that the
  // model asked us to commit.  May clobber state related to the
  // model's previous commit attempt(s).
  void RequestCommit(const std::string& id,
                     const std::string& client_tag_hash,
                     int64 sequence_number,
                     int64 base_version,
                     base::Time ctime,
                     base::Time mtime,
                     const std::string& non_unique_name,
                     bool deleted,
                     const sync_pb::EntitySpecifics& specifics);

  // Handles the receipt of a commit response.
  //
  // Since commits happen entirely on the sync thread, we can safely assume
  // that our item's state at the end of the commit is the same as it was at
  // the start.
  void ReceiveCommitResponse(const std::string& response_id,
                             int64 response_version,
                             int64 sequence_number);

  // Handles receipt of an update from the server.
  void ReceiveUpdate(int64 version);

  // Handles the receipt of an pending update from the server.
  //
  // Returns true if the tracker decides this item is worth keeping.  Returns
  // false if the item is discarded, which could happen if the version number
  // is out of date.
  bool ReceivePendingUpdate(const UpdateResponseData& data);

  // Functions to fetch the latest pending update.
  bool HasPendingUpdate() const;
  UpdateResponseData GetPendingUpdate() const;

  // Clears the pending update.  Allows us to resume regular commit behavior.
  void ClearPendingUpdate();

 private:
  // Initializes received update state.  Does not initialize state related to
  // pending commits and sets |is_commit_pending_| to false.
  EntityTracker(const std::string& id,
                const std::string& client_tag_hash,
                int64 highest_commit_response_version,
                int64 highest_gu_response_version);

  // Initializes all fields.  Sets |is_commit_pending_| to true.
  EntityTracker(const std::string& id,
                const std::string& client_tag_hash,
                int64 highest_commit_response_version,
                int64 highest_gu_response_version,
                bool is_commit_pending,
                int64 sequence_number,
                int64 base_version,
                base::Time ctime,
                base::Time mtime,
                const std::string& non_unique_name,
                bool deleted,
                const sync_pb::EntitySpecifics& specifics);

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
  int64 highest_commit_response_version_;

  // The highest version seen in a GU response for this entry.
  int64 highest_gu_response_version_;

  // Flag that indicates whether or not we're waiting for a chance to commit
  // this item.
  bool is_commit_pending_;

  // Used to track in-flight commit requests on the model thread.  All we need
  // to do here is return it back to the model thread when the pending commit
  // is completed and confirmed.  Not valid if no commit is pending.
  int64 sequence_number_;

  // The following fields are valid only when a commit is pending.
  // This is where we store the data that is to be sent up to the server
  // at the next possible opportunity.
  int64 base_version_;
  base::Time ctime_;
  base::Time mtime_;
  std::string non_unique_name_;
  bool deleted_;
  sync_pb::EntitySpecifics specifics_;

  // An update for this item which can't be applied right now.  The presence of
  // an pending update prevents commits.  As of this writing, the only source
  // of pending updates is updates we can't decrypt right now.
  scoped_ptr<UpdateResponseData> pending_update_;

  DISALLOW_COPY_AND_ASSIGN(EntityTracker);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_ENTITY_TRACKER_H_
