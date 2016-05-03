// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/worker_entity_tracker.h"

#include <stdint.h>

#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer_v2 {

WorkerEntityTracker::WorkerEntityTracker(const std::string& id,
                                         const std::string& client_tag_hash)
    : id_(id),
      client_tag_hash_(client_tag_hash),
      highest_commit_response_version_(0),
      highest_gu_response_version_(0),
      sequence_number_(0),
      base_version_(kUncommittedVersion) {
  DCHECK(!client_tag_hash_.empty());
}

WorkerEntityTracker::~WorkerEntityTracker() {}

bool WorkerEntityTracker::HasPendingCommit() const {
  return !!pending_commit_;
}

void WorkerEntityTracker::PopulateCommitProto(
    sync_pb::SyncEntity* commit_entity) const {
  DCHECK(HasPendingCommit());
  DCHECK(!client_tag_hash_.empty());

  if (!id_.empty()) {
    commit_entity->set_id_string(id_);
  }

  const EntityData& entity = pending_commit_->entity.value();
  DCHECK_EQ(client_tag_hash_, entity.client_tag_hash);

  commit_entity->set_client_defined_unique_tag(client_tag_hash_);
  commit_entity->set_version(base_version_);
  commit_entity->set_deleted(entity.is_deleted());

  // TODO(stanisc): This doesn't support bookmarks yet.
  DCHECK(entity.parent_id.empty());
  commit_entity->set_folder(false);

  commit_entity->set_name(entity.non_unique_name);
  if (!entity.is_deleted()) {
    commit_entity->set_ctime(syncer::TimeToProtoTime(entity.creation_time));
    commit_entity->set_mtime(syncer::TimeToProtoTime(entity.modification_time));
    commit_entity->mutable_specifics()->CopyFrom(entity.specifics);
  }
}

void WorkerEntityTracker::RequestCommit(const CommitRequestData& data) {
  DCHECK_GE(data.base_version, base_version_)
      << "Base version should never decrease";

  DCHECK_GE(data.sequence_number, sequence_number_)
      << "Sequence number should never decrease";

  // Update our book-keeping counters.
  base_version_ = data.base_version;
  sequence_number_ = data.sequence_number;
  pending_commit_specifics_hash_ = data.specifics_hash;

  // Don't commit deletions of server-unknown items.
  if (data.entity->is_deleted() && !IsServerKnown()) {
    ClearPendingCommit();
    return;
  }

  // We intentionally don't update the id_ here.  Good ID values come from the
  // server and always pass through the sync thread first.  There's no way the
  // model thread could have a better ID value than we do.

  // This entity is identified by its client tag.  That value can never change.
  DCHECK_EQ(client_tag_hash_, data.entity->client_tag_hash);
  // TODO(stanisc): consider simply copying CommitRequestData instead of
  // allocating one dynamically.
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

void WorkerEntityTracker::ReceiveCommitResponse(CommitResponseData* ack) {
  DCHECK_GT(ack->response_version, highest_commit_response_version_)
      << "Had expected higher response version."
      << " id: " << id_;

  // Commit responses, especially after the first commit, can update our ID.
  id_ = ack->id;
  highest_commit_response_version_ = ack->response_version;

  // Fill in some cached info for the response data. Since commits happen
  // synchronously on the sync thread, our item's state is guaranteed to be
  // the same at the end of the commit as it was at the start.
  ack->sequence_number = sequence_number_;
  ack->specifics_hash = pending_commit_specifics_hash_;

  // Because an in-progress commit blocks the sync thread, we can assume that
  // the item we just committed successfully is exactly the one we have now.
  // Nothing changed it while the commit was happening.  Since we're now in
  // sync with the server, we can clear the pending commit.
  ClearPendingCommit();
}

void WorkerEntityTracker::ReceiveUpdate(int64_t version) {
  if (version <= highest_gu_response_version_)
    return;

  highest_gu_response_version_ = version;

  // Got an applicable update newer than any pending updates.  It must be safe
  // to discard the old encrypted update, if there was one.
  ClearEncryptedUpdate();

  if (IsInConflict()) {
    // Incoming update clobbers the pending commit on the sync thread.
    // The model thread can re-request this commit later if it wants to.
    ClearPendingCommit();
  }
}

bool WorkerEntityTracker::ReceiveEncryptedUpdate(
    const UpdateResponseData& data) {
  if (data.response_version < highest_gu_response_version_)
    return false;

  highest_gu_response_version_ = data.response_version;
  encrypted_update_.reset(new UpdateResponseData(data));
  ClearPendingCommit();
  return true;
}

bool WorkerEntityTracker::HasEncryptedUpdate() const {
  return !!encrypted_update_;
}

UpdateResponseData WorkerEntityTracker::GetEncryptedUpdate() const {
  return *encrypted_update_;
}

void WorkerEntityTracker::ClearEncryptedUpdate() {
  encrypted_update_.reset();
}

bool WorkerEntityTracker::IsInConflict() const {
  if (!HasPendingCommit())
    return false;

  if (HasEncryptedUpdate())
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

bool WorkerEntityTracker::IsServerKnown() const {
  return base_version_ != kUncommittedVersion;
}

void WorkerEntityTracker::ClearPendingCommit() {
  pending_commit_.reset();
  pending_commit_specifics_hash_.clear();
}

}  // namespace syncer_v2
