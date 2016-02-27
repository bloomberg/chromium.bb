// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_entity.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/sha1.h"
#include "base/time/time.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer_v2 {

scoped_ptr<ModelTypeEntity> ModelTypeEntity::CreateNew(
    const std::string& client_tag,
    const std::string& client_tag_hash,
    const std::string& id,
    base::Time creation_time) {
  // Initialize metadata
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(client_tag_hash);
  if (!id.empty())
    metadata.set_server_id(id);
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(0);
  metadata.set_server_version(kUncommittedVersion);
  metadata.set_creation_time(syncer::TimeToProtoTime(creation_time));

  return scoped_ptr<ModelTypeEntity>(
      new ModelTypeEntity(client_tag, &metadata));
}

scoped_ptr<ModelTypeEntity> ModelTypeEntity::CreateFromMetadata(
    const std::string& client_tag,
    sync_pb::EntityMetadata* metadata) {
  return scoped_ptr<ModelTypeEntity>(new ModelTypeEntity(client_tag, metadata));
}

ModelTypeEntity::ModelTypeEntity(const std::string& client_tag,
                                 sync_pb::EntityMetadata* metadata)
    : client_tag_(client_tag),
      commit_requested_sequence_number_(metadata->acked_sequence_number()) {
  DCHECK(metadata->has_client_tag_hash());
  DCHECK(metadata->has_creation_time());
  metadata_.Swap(metadata);
}

ModelTypeEntity::~ModelTypeEntity() {}

void ModelTypeEntity::CacheCommitData(EntityData* data) {
  DCHECK(RequiresCommitRequest());
  if (data->client_tag_hash.empty()) {
    data->client_tag_hash = metadata_.client_tag_hash();
  }
  commit_data_ = data->PassToPtr();
  DCHECK(HasCommitData());
}

bool ModelTypeEntity::HasCommitData() const {
  return !commit_data_->client_tag_hash.empty();
}

bool ModelTypeEntity::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

bool ModelTypeEntity::RequiresCommitRequest() const {
  return metadata_.sequence_number() > commit_requested_sequence_number_;
}

bool ModelTypeEntity::RequiresCommitData() const {
  return RequiresCommitRequest() && !HasCommitData() && !metadata_.is_deleted();
}

bool ModelTypeEntity::UpdateIsReflection(int64_t update_version) const {
  return metadata_.server_version() >= update_version;
}

bool ModelTypeEntity::UpdateIsInConflict(int64_t update_version) const {
  return IsUnsynced() && !UpdateIsReflection(update_version);
}

void ModelTypeEntity::ApplyUpdateFromServer(
    const UpdateResponseData& response_data) {
  DCHECK(metadata_.has_client_tag_hash());
  DCHECK(!metadata_.client_tag_hash().empty());
  DCHECK(metadata_.has_sequence_number());

  // TODO(stanisc): crbug/561829: Filter out update if specifics hash hasn't
  // changed.

  // TODO(stanisc): crbug/521867: Understand and verify the conflict resolution
  // logic here.
  // There was a conflict and the server just won it.
  // This implicitly acks all outstanding commits because a received update
  // will clobber any pending commits on the sync thread.
  metadata_.set_acked_sequence_number(metadata_.sequence_number());
  commit_requested_sequence_number_ = metadata_.sequence_number();

  metadata_.set_server_version(response_data.response_version);
  metadata_.set_modification_time(
      syncer::TimeToProtoTime(response_data.entity->modification_time));
  UpdateSpecificsHash(response_data.entity->specifics);

  encryption_key_name_ = response_data.encryption_key_name;
}

void ModelTypeEntity::MakeLocalChange(scoped_ptr<EntityData> data) {
  DCHECK(!metadata_.client_tag_hash().empty());
  DCHECK_EQ(metadata_.client_tag_hash(), data->client_tag_hash);
  DCHECK(!data->modification_time.is_null());

  metadata_.set_modification_time(
      syncer::TimeToProtoTime(data->modification_time));
  metadata_.set_is_deleted(false);
  IncrementSequenceNumber();
  UpdateSpecificsHash(data->specifics);

  data->id = metadata_.server_id();
  data->creation_time = syncer::ProtoTimeToTime(metadata_.creation_time());
  CacheCommitData(data.get());
}

void ModelTypeEntity::UpdateDesiredEncryptionKey(const std::string& name) {
  if (encryption_key_name_ == name)
    return;

  DVLOG(2) << metadata_.server_id()
           << ": Encryption triggered commit: " << encryption_key_name_
           << " -> " << name;

  // Schedule commit with the expectation that the worker will re-encrypt with
  // the latest encryption key as it does.
  IncrementSequenceNumber();
}

void ModelTypeEntity::Delete() {
  IncrementSequenceNumber();
  metadata_.set_is_deleted(true);
  metadata_.clear_specifics_hash();
  // Clear any cached pending commit data.
  if (HasCommitData()) commit_data_.reset();
}

void ModelTypeEntity::InitializeCommitRequestData(CommitRequestData* request) {
  if (!metadata_.is_deleted()) {
    DCHECK(HasCommitData());
    DCHECK_EQ(commit_data_->client_tag_hash, metadata_.client_tag_hash());
    request->entity = commit_data_;
  } else {
    // Make an EntityData with empty specifics to indicate deletion. This is
    // done lazily here to simplify loading a pending deletion on startup.
    EntityData data;
    data.client_tag_hash = metadata_.client_tag_hash();
    data.id = metadata_.server_id();
    data.creation_time = syncer::ProtoTimeToTime(metadata_.creation_time());
    request->entity = data.PassToPtr();
  }

  request->sequence_number = metadata_.sequence_number();
  request->base_version = metadata_.server_version();
  commit_requested_sequence_number_ = metadata_.sequence_number();
}

void ModelTypeEntity::ReceiveCommitResponse(
    const std::string& id,
    int64_t sequence_number,
    int64_t response_version,
    const std::string& encryption_key_name) {
  // The server can assign us a new ID in a commit response.
  metadata_.set_server_id(id);
  metadata_.set_acked_sequence_number(sequence_number);
  metadata_.set_server_version(response_version);
  encryption_key_name_ = encryption_key_name;
}

void ModelTypeEntity::ClearTransientSyncState() {
  // If we have any unacknowledged commit requests outstanding, they've been
  // dropped and we should forget about them.
  commit_requested_sequence_number_ = metadata_.acked_sequence_number();
}

void ModelTypeEntity::IncrementSequenceNumber() {
  DCHECK(metadata_.has_sequence_number());
  metadata_.set_sequence_number(metadata_.sequence_number() + 1);
}

// Update hash string for EntitySpecifics.
void ModelTypeEntity::UpdateSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.ByteSize() > 0) {
    std::string hash_input;
    specifics.AppendToString(&hash_input);
    base::Base64Encode(base::SHA1HashString(hash_input),
                       metadata_.mutable_specifics_hash());
  } else {
    metadata_.clear_specifics_hash();
  }
}

}  // namespace syncer_v2
