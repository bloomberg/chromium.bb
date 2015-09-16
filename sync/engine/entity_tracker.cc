// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/entity_tracker.h"

#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer_v2 {

scoped_ptr<EntityTracker> EntityTracker::FromUpdateResponse(
    const UpdateResponseData& data) {
  return make_scoped_ptr(new EntityTracker(data.id, data.client_tag_hash, 0,
                                           data.response_version));
}

scoped_ptr<EntityTracker> EntityTracker::FromCommitRequest(
    const CommitRequestData& data) {
  return make_scoped_ptr(
      new EntityTracker(data.id, data.client_tag_hash, 0, 0));
}

EntityTracker::EntityTracker(const std::string& id,
                             const std::string& client_tag_hash,
                             int64 highest_commit_response_version,
                             int64 highest_gu_response_version)
    : id_(id),
      client_tag_hash_(client_tag_hash),
      highest_commit_response_version_(highest_commit_response_version),
      highest_gu_response_version_(highest_gu_response_version),
      sequence_number_(0),
      base_version_(kUncommittedVersion) {}

EntityTracker::~EntityTracker() {}

bool EntityTracker::HasPendingCommit() const {
  return !!pending_commit_;
}

void EntityTracker::PrepareCommitProto(sync_pb::SyncEntity* commit_entity,
                                       int64* sequence_number) const {
  DCHECK(HasPendingCommit());

  // Set ID if we have a server-assigned ID.  Otherwise, it will be up to
  // our caller to assign a client-unique initial ID.
  if (base_version_ != kUncommittedVersion) {
    commit_entity->set_id_string(id_);
  }

  commit_entity->set_client_defined_unique_tag(client_tag_hash_);
  commit_entity->set_version(base_version_);
  commit_entity->set_deleted(pending_commit_->deleted);
  commit_entity->set_folder(false);
  commit_entity->set_name(pending_commit_->non_unique_name);
  if (!pending_commit_->deleted) {
    commit_entity->set_ctime(syncer::TimeToProtoTime(pending_commit_->ctime));
    commit_entity->set_mtime(syncer::TimeToProtoTime(pending_commit_->mtime));
    commit_entity->mutable_specifics()->CopyFrom(pending_commit_->specifics);
  }

  *sequence_number = sequence_number_;
}

void EntityTracker::RequestCommit(const CommitRequestData& data) {
  DCHECK_GE(data.base_version, base_version_)
      << "Base version should never decrease";

  DCHECK_GE(data.sequence_number, sequence_number_)
      << "Sequence number should never decrease";

  // Update our book-keeping counters.
  base_version_ = data.base_version;
  sequence_number_ = data.sequence_number;

  // Don't commit deletions of server-unknown items.
  if (data.deleted && !IsServerKnown()) {
    ClearPendingCommit();
    return;
  }

  // We intentionally don't update the id_ here.  Good ID values come from the
  // server and always pass through the sync thread first.  There's no way the
  // model thread could have a better ID value than we do.

  // This entity is identified by its client tag.  That value can never change.
  DCHECK_EQ(client_tag_hash_, data.client_tag_hash);
  pending_commit_.reset(new CommitRequestData(data));

  // Do our counter values indicate a conflict?  If so, don't commit.
  //
  // There's no need to inform the model thread of the conflict.  The
  // conflicting update has already been posted to its task runner; it will
  // figure it out as soon as it runs that task.
  //
  // Note that this check must be after pending_commit_ is set.
  if (IsInConflict()) {
    ClearPendingCommit();
    return;
  }

  // Otherwise, keep the data associated with this pending commit
  // so it can be committed at the next possible opportunity.
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
  if (version <= highest_gu_response_version_)
    return;

  highest_gu_response_version_ = version;

  // Got an applicable update newer than any pending updates.  It must be safe
  // to discard the old pending update, if there was one.
  ClearPendingUpdate();

  if (IsInConflict()) {
    // Incoming update clobbers the pending commit on the sync thread.
    // The model thread can re-request this commit later if it wants to.
    ClearPendingCommit();
  }
}

bool EntityTracker::ReceivePendingUpdate(const UpdateResponseData& data) {
  if (data.response_version < highest_gu_response_version_)
    return false;

  highest_gu_response_version_ = data.response_version;
  pending_update_.reset(new UpdateResponseData(data));
  ClearPendingCommit();
  return true;
}

bool EntityTracker::HasPendingUpdate() const {
  return !!pending_update_;
}

UpdateResponseData EntityTracker::GetPendingUpdate() const {
  return *pending_update_;
}

void EntityTracker::ClearPendingUpdate() {
  pending_update_.reset();
}

bool EntityTracker::IsInConflict() const {
  if (!HasPendingCommit())
    return false;

  if (HasPendingUpdate())
    return true;

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
  pending_commit_.reset();
}

}  // namespace syncer
