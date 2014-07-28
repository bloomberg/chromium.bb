// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/entity_tracker.h"

#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer {

EntityTracker* EntityTracker::FromServerUpdate(
    const std::string& id_string,
    const std::string& client_tag_hash,
    int64 received_version) {
  return new EntityTracker(id_string, client_tag_hash, 0, received_version);
}

EntityTracker* EntityTracker::FromCommitRequest(
    const std::string& id_string,
    const std::string& client_tag_hash,
    int64 sequence_number,
    int64 base_version,
    base::Time ctime,
    base::Time mtime,
    const std::string& non_unique_name,
    bool deleted,
    const sync_pb::EntitySpecifics& specifics) {
  return new EntityTracker(id_string,
                           client_tag_hash,
                           0,
                           0,
                           true,
                           sequence_number,
                           base_version,
                           ctime,
                           mtime,
                           non_unique_name,
                           deleted,
                           specifics);
}

// Constructor that does not set any pending commit fields.
EntityTracker::EntityTracker(const std::string& id,
                             const std::string& client_tag_hash,
                             int64 highest_commit_response_version,
                             int64 highest_gu_response_version)
    : id_(id),
      client_tag_hash_(client_tag_hash),
      highest_commit_response_version_(highest_commit_response_version),
      highest_gu_response_version_(highest_gu_response_version),
      is_commit_pending_(false),
      sequence_number_(0),
      base_version_(0),
      deleted_(false) {
}

EntityTracker::EntityTracker(const std::string& id,
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
                             const sync_pb::EntitySpecifics& specifics)
    : id_(id),
      client_tag_hash_(client_tag_hash),
      highest_commit_response_version_(highest_commit_response_version),
      highest_gu_response_version_(highest_gu_response_version),
      is_commit_pending_(is_commit_pending),
      sequence_number_(sequence_number),
      base_version_(base_version),
      ctime_(ctime),
      mtime_(mtime),
      non_unique_name_(non_unique_name),
      deleted_(deleted),
      specifics_(specifics) {
}

EntityTracker::~EntityTracker() {
}

bool EntityTracker::IsCommitPending() const {
  return is_commit_pending_;
}

void EntityTracker::PrepareCommitProto(sync_pb::SyncEntity* commit_entity,
                                       int64* sequence_number) const {
  // Set ID if we have a server-assigned ID.  Otherwise, it will be up to
  // our caller to assign a client-unique initial ID.
  if (base_version_ != kUncommittedVersion) {
    commit_entity->set_id_string(id_);
  }

  commit_entity->set_client_defined_unique_tag(client_tag_hash_);
  commit_entity->set_version(base_version_);
  commit_entity->set_deleted(deleted_);
  commit_entity->set_folder(false);
  commit_entity->set_name(non_unique_name_);
  if (!deleted_) {
    commit_entity->set_ctime(TimeToProtoTime(ctime_));
    commit_entity->set_mtime(TimeToProtoTime(mtime_));
    commit_entity->mutable_specifics()->CopyFrom(specifics_);
  }

  *sequence_number = sequence_number_;
}

void EntityTracker::RequestCommit(const std::string& id,
                                  const std::string& client_tag_hash,
                                  int64 sequence_number,
                                  int64 base_version,
                                  base::Time ctime,
                                  base::Time mtime,
                                  const std::string& non_unique_name,
                                  bool deleted,
                                  const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GE(base_version, base_version_)
      << "Base version should never decrease";

  DCHECK_GE(sequence_number, sequence_number_)
      << "Sequence number should never decrease";

  // Update our book-keeping counters.
  base_version_ = base_version;
  sequence_number_ = sequence_number;

  // Do our counter values indicate a conflict?  If so, don't commit.
  //
  // There's no need to inform the model thread of the conflict.  The
  // conflicting update has already been posted to its task runner; it will
  // figure it out as soon as it runs that task.
  is_commit_pending_ = true;
  if (IsInConflict()) {
    ClearPendingCommit();
    return;
  }

  // We don't commit deletions of server-unknown items.
  if (deleted && !IsServerKnown()) {
    ClearPendingCommit();
    return;
  }

  // Otherwise, we should store the data associated with this pending commit
  // so we're ready to commit at the next possible opportunity.

  // We intentionally don't update the id_ here.  Good ID values come from the
  // server and always pass through the sync thread first.  There's no way the
  // model thread could have a better ID value than we do.

  // This entity is identified by its client tag.  That value can never change.
  DCHECK_EQ(client_tag_hash_, client_tag_hash);

  // Set the fields for the pending commit.
  ctime_ = ctime;
  mtime_ = mtime;
  non_unique_name_ = non_unique_name;
  deleted_ = deleted;
  specifics_ = specifics;
}

void EntityTracker::ReceiveCommitResponse(const std::string& response_id,
                                          int64 response_version,
                                          int64 sequence_number) {
  // Commit responses, especially after the first commit, can update our ID.
  id_ = response_id;

  DCHECK_GT(response_version, highest_commit_response_version_)
      << "Had expected higher response version."
      << " id: " << id_;

  // Commits are synchronous, so there's no reason why the sequence numbers
  // wouldn't match.
  DCHECK_EQ(sequence_number_, sequence_number)
      << "Unexpected sequence number mismatch."
      << " id: " << id_;

  highest_commit_response_version_ = response_version;

  // Because an in-progress commit blocks the sync thread, we can assume that
  // the item we just committed successfully is exactly the one we have now.
  // Nothing changed it while the commit was happening.  Since we're now in
  // sync with the server, we can clear the pending commit.
  ClearPendingCommit();
}

void EntityTracker::ReceiveUpdate(int64 version) {
  highest_gu_response_version_ =
      std::max(highest_gu_response_version_, version);

  if (IsInConflict()) {
    // Incoming update clobbers the pending commit on the sync thread.
    // The model thread can re-request this commit later if it wants to.
    ClearPendingCommit();
  }
}

bool EntityTracker::IsInConflict() const {
  if (!is_commit_pending_)
    return false;

  if (highest_gu_response_version_ <= highest_commit_response_version_) {
    // The most recent server state was created in a commit made by this
    // client.  We're fully up to date, and therefore not in conflict.
    return false;
  } else {
    // The most recent server state was written by someone else.
    // Did the model thread have the most up to date version when it issued the
    // commit request?
    if (base_version_ >= highest_gu_response_version_) {
      return false;  // Yes.
    } else {
      return true;  // No.
    }
  }
}

bool EntityTracker::IsServerKnown() const {
  return base_version_ != kUncommittedVersion;
}

void EntityTracker::ClearPendingCommit() {
  is_commit_pending_ = false;

  // Clearing the specifics might free up some memory.  It can't hurt to try.
  specifics_.Clear();
}

}  // namespace syncer
