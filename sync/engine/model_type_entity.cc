// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_entity.h"
#include "sync/syncable/syncable_util.h"

namespace syncer {

scoped_ptr<ModelTypeEntity> ModelTypeEntity::NewLocalItem(
    const std::string& client_tag,
    const sync_pb::EntitySpecifics& specifics,
    base::Time now) {
  return scoped_ptr<ModelTypeEntity>(new ModelTypeEntity(
      1,
      0,
      0,
      kUncommittedVersion,
      true,
      std::string(),  // Sync thread will assign the initial ID.
      syncable::GenerateSyncableHash(GetModelTypeFromSpecifics(specifics),
                                     client_tag),
      client_tag,  // As non-unique name.
      specifics,
      false,
      now,
      now,
      std::string()));
}

scoped_ptr<ModelTypeEntity> ModelTypeEntity::FromServerUpdate(
    const std::string& id,
    const std::string& client_tag_hash,
    const std::string& non_unique_name,
    int64 version,
    const sync_pb::EntitySpecifics& specifics,
    bool deleted,
    base::Time ctime,
    base::Time mtime,
    const std::string& encryption_key_name) {
  return scoped_ptr<ModelTypeEntity>(new ModelTypeEntity(0,
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
                                                         mtime,
                                                         encryption_key_name));
}

ModelTypeEntity::ModelTypeEntity(int64 sequence_number,
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
                                 base::Time mtime,
                                 const std::string& encryption_key_name)
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
      mtime_(mtime),
      encryption_key_name_(encryption_key_name) {
}

ModelTypeEntity::~ModelTypeEntity() {
}

bool ModelTypeEntity::IsWriteRequired() const {
  return is_dirty_;
}

bool ModelTypeEntity::IsUnsynced() const {
  return sequence_number_ > acked_sequence_number_;
}

bool ModelTypeEntity::RequiresCommitRequest() const {
  return sequence_number_ > commit_requested_sequence_number_;
}

bool ModelTypeEntity::UpdateIsReflection(int64 update_version) const {
  return base_version_ >= update_version;
}

bool ModelTypeEntity::UpdateIsInConflict(int64 update_version) const {
  return IsUnsynced() && !UpdateIsReflection(update_version);
}

void ModelTypeEntity::ApplyUpdateFromServer(
    int64 update_version,
    bool deleted,
    const sync_pb::EntitySpecifics& specifics,
    base::Time mtime,
    const std::string& encryption_key_name) {
  // There was a conflict and the server just won it.
  // This implicitly acks all outstanding commits because a received update
  // will clobber any pending commits on the sync thread.
  acked_sequence_number_ = sequence_number_;
  commit_requested_sequence_number_ = sequence_number_;

  base_version_ = update_version;
  specifics_ = specifics;
  mtime_ = mtime;
}

void ModelTypeEntity::MakeLocalChange(
    const sync_pb::EntitySpecifics& specifics) {
  sequence_number_++;
  specifics_ = specifics;
}

void ModelTypeEntity::UpdateDesiredEncryptionKey(const std::string& name) {
  if (encryption_key_name_ == name)
    return;

  // Schedule commit with the expectation that the worker will re-encrypt with
  // the latest encryption key as it does.
  sequence_number_++;
}

void ModelTypeEntity::Delete() {
  sequence_number_++;
  specifics_.Clear();
  deleted_ = true;
}

void ModelTypeEntity::InitializeCommitRequestData(
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

void ModelTypeEntity::SetCommitRequestInProgress() {
  commit_requested_sequence_number_ = sequence_number_;
}

void ModelTypeEntity::ReceiveCommitResponse(
    const std::string& id,
    int64 sequence_number,
    int64 response_version,
    const std::string& encryption_key_name) {
  id_ = id;  // The server can assign us a new ID in a commit response.
  acked_sequence_number_ = sequence_number;
  base_version_ = response_version;
  encryption_key_name_ = encryption_key_name;
}

void ModelTypeEntity::ClearTransientSyncState() {
  // If we have any unacknowledged commit requests outstatnding, they've been
  // dropped and we should forget about them.
  commit_requested_sequence_number_ = acked_sequence_number_;
}

void ModelTypeEntity::ClearSyncState() {
  base_version_ = kUncommittedVersion;
  is_dirty_ = true;
  sequence_number_ = 1;
  commit_requested_sequence_number_ = 0;
  acked_sequence_number_ = 0;
  id_.clear();
}

}  // namespace syncer
