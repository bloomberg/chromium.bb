// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_thread_sync_entity.h"
#include "sync/syncable/syncable_util.h"

namespace syncer {

scoped_ptr<ModelThreadSyncEntity> ModelThreadSyncEntity::NewLocalItem(
    const std::string& client_tag,
    const sync_pb::EntitySpecifics& specifics,
    base::Time now) {
  return scoped_ptr<ModelThreadSyncEntity>(new ModelThreadSyncEntity(
      1,
      0,
      0,
      0,
      true,
      std::string(),  // Sync thread will assign the initial ID.
      syncable::GenerateSyncableHash(GetModelTypeFromSpecifics(specifics),
                                     client_tag),
      client_tag,  // As non-unique name.
      specifics,
      false,
      now,
      now));
}

scoped_ptr<ModelThreadSyncEntity> ModelThreadSyncEntity::FromServerUpdate(
    const std::string& id,
    const std::string& client_tag_hash,
    const std::string& non_unique_name,
    int64 version,
    const sync_pb::EntitySpecifics& specifics,
    bool deleted,
    base::Time ctime,
    base::Time mtime) {
  return scoped_ptr<ModelThreadSyncEntity>(
      new ModelThreadSyncEntity(0,
                                0,
                                0,
                                version,
                                true,
                                id,
                                client_tag_hash,
                                non_unique_name,
                                specifics,
                                deleted,
                                ctime,
                                mtime));
}

ModelThreadSyncEntity::ModelThreadSyncEntity(
    int64 sequence_number,
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
    base::Time mtime)
    : sequence_number_(sequence_number),
      commit_requested_sequence_number_(commit_requested_sequence_number),
      acked_sequence_number_(acked_sequence_number),
      base_version_(base_version),
      is_dirty_(is_dirty),
      id_(id),
      client_tag_hash_(client_tag_hash),
      non_unique_name_(non_unique_name),
      specifics_(specifics),
      deleted_(deleted),
      ctime_(ctime),
      mtime_(mtime) {
}

ModelThreadSyncEntity::~ModelThreadSyncEntity() {
}

bool ModelThreadSyncEntity::IsWriteRequired() const {
  return is_dirty_;
}

bool ModelThreadSyncEntity::IsUnsynced() const {
  return sequence_number_ > acked_sequence_number_;
}

bool ModelThreadSyncEntity::RequiresCommitRequest() const {
  return sequence_number_ > commit_requested_sequence_number_;
}

bool ModelThreadSyncEntity::UpdateIsReflection(int64 update_version) const {
  return base_version_ >= update_version;
}

bool ModelThreadSyncEntity::UpdateIsInConflict(int64 update_version) const {
  return IsUnsynced() && !UpdateIsReflection(update_version);
}

void ModelThreadSyncEntity::ApplyUpdateFromServer(
    int64 update_version,
    bool deleted,
    const sync_pb::EntitySpecifics& specifics,
    base::Time mtime) {
  // There was a conflict and the server just won it.
  // This implicitly acks all outstanding commits because a received update
  // will clobber any pending commits on the sync thread.
  acked_sequence_number_ = sequence_number_;
  commit_requested_sequence_number_ = sequence_number_;

  base_version_ = update_version;
  specifics_ = specifics;
  mtime_ = mtime;
}

void ModelThreadSyncEntity::MakeLocalChange(
    const sync_pb::EntitySpecifics& specifics) {
  sequence_number_++;
  specifics_ = specifics;
}

void ModelThreadSyncEntity::Delete() {
  sequence_number_++;
  specifics_.Clear();
  deleted_ = true;
}

void ModelThreadSyncEntity::InitializeCommitRequestData(
    CommitRequestData* request) const {
  request->id = id_;
  request->client_tag_hash = client_tag_hash_;
  request->sequence_number = sequence_number_;
  request->base_version = base_version_;
  request->ctime = ctime_;
  request->mtime = mtime_;
  request->non_unique_name = non_unique_name_;
  request->deleted = deleted_;
  request->specifics.CopyFrom(specifics_);
}

void ModelThreadSyncEntity::SetCommitRequestInProgress() {
  commit_requested_sequence_number_ = sequence_number_;
}

void ModelThreadSyncEntity::ReceiveCommitResponse(const std::string& id,
                                                  int64 sequence_number,
                                                  int64 response_version) {
  id_ = id;  // The server can assign us a new ID in a commit response.
  acked_sequence_number_ = sequence_number;
  base_version_ = response_version;
}

}  // namespace syncer
