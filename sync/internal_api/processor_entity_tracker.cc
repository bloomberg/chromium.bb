// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/processor_entity_tracker.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/sha1.h"
#include "base/time/time.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer_v2 {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()), hash);
}

}  // namespace

std::unique_ptr<ProcessorEntityTracker> ProcessorEntityTracker::CreateNew(
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

  return std::unique_ptr<ProcessorEntityTracker>(
      new ProcessorEntityTracker(client_tag, &metadata));
}

std::unique_ptr<ProcessorEntityTracker>
ProcessorEntityTracker::CreateFromMetadata(const std::string& client_tag,
                                           sync_pb::EntityMetadata* metadata) {
  return std::unique_ptr<ProcessorEntityTracker>(
      new ProcessorEntityTracker(client_tag, metadata));
}

ProcessorEntityTracker::ProcessorEntityTracker(
    const std::string& client_tag,
    sync_pb::EntityMetadata* metadata)
    : client_tag_(client_tag),
      commit_requested_sequence_number_(metadata->acked_sequence_number()) {
  DCHECK(metadata->has_client_tag_hash());
  DCHECK(metadata->has_creation_time());
  metadata_.Swap(metadata);
}

ProcessorEntityTracker::~ProcessorEntityTracker() {}

void ProcessorEntityTracker::CacheCommitData(EntityData* data) {
  DCHECK(data);
  if (data->client_tag_hash.empty()) {
    data->client_tag_hash = metadata_.client_tag_hash();
  }
  CacheCommitData(data->PassToPtr());
}

void ProcessorEntityTracker::CacheCommitData(const EntityDataPtr& data_ptr) {
  DCHECK(RequiresCommitData());
  commit_data_ = data_ptr;
  DCHECK(HasCommitData());
}

bool ProcessorEntityTracker::HasCommitData() const {
  return !commit_data_->client_tag_hash.empty();
}

bool ProcessorEntityTracker::MatchesData(const EntityData& data) const {
  return metadata_.is_deleted() ? data.is_deleted() :
         MatchesSpecificsHash(data.specifics);
}

bool ProcessorEntityTracker::MatchesBaseData(const EntityData& data) const {
  DCHECK(IsUnsynced());
  if (data.is_deleted() || metadata_.base_specifics_hash().empty()) {
    return false;
  }
  std::string hash;
  HashSpecifics(data.specifics, &hash);
  return hash == metadata_.base_specifics_hash();
}

bool ProcessorEntityTracker::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

bool ProcessorEntityTracker::RequiresCommitRequest() const {
  return metadata_.sequence_number() > commit_requested_sequence_number_;
}

bool ProcessorEntityTracker::RequiresCommitData() const {
  return RequiresCommitRequest() && !HasCommitData() && !metadata_.is_deleted();
}

bool ProcessorEntityTracker::CanClearMetadata() const {
  return metadata_.is_deleted() && !IsUnsynced();
}

bool ProcessorEntityTracker::UpdateIsReflection(int64_t update_version) const {
  return metadata_.server_version() >= update_version;
}

void ProcessorEntityTracker::RecordIgnoredUpdate(
    const UpdateResponseData& update) {
  metadata_.set_server_version(update.response_version);
  // Either these already matched, acked was just bumped to squash a pending
  // commit and this should follow, or the pending commit needs to be requeued.
  commit_requested_sequence_number_ = metadata_.acked_sequence_number();
}

void ProcessorEntityTracker::RecordAcceptedUpdate(
    const UpdateResponseData& update) {
  DCHECK(!IsUnsynced());
  RecordIgnoredUpdate(update);
  metadata_.set_is_deleted(update.entity->is_deleted());
  metadata_.set_modification_time(
      syncer::TimeToProtoTime(update.entity->modification_time));
  UpdateSpecificsHash(update.entity->specifics);
}

void ProcessorEntityTracker::RecordForcedUpdate(
    const UpdateResponseData& update) {
  DCHECK(IsUnsynced());
  // There was a conflict and the server just won it. Explicitly ack all
  // pending commits so they are never enqueued again.
  metadata_.set_acked_sequence_number(metadata_.sequence_number());
  commit_data_.reset();
  RecordAcceptedUpdate(update);
}

void ProcessorEntityTracker::MakeLocalChange(std::unique_ptr<EntityData> data) {
  DCHECK(!metadata_.client_tag_hash().empty());
  DCHECK_EQ(metadata_.client_tag_hash(), data->client_tag_hash);

  if (data->modification_time.is_null()) {
    data->modification_time = base::Time::Now();
  }

  IncrementSequenceNumber();
  UpdateSpecificsHash(data->specifics);
  metadata_.set_modification_time(
      syncer::TimeToProtoTime(data->modification_time));
  metadata_.set_is_deleted(false);

  data->id = metadata_.server_id();
  data->creation_time = syncer::ProtoTimeToTime(metadata_.creation_time());
  commit_data_.reset();
  CacheCommitData(data.get());
}

void ProcessorEntityTracker::Delete() {
  IncrementSequenceNumber();
  metadata_.set_is_deleted(true);
  metadata_.clear_specifics_hash();
  // Clear any cached pending commit data.
  commit_data_.reset();
}

void ProcessorEntityTracker::InitializeCommitRequestData(
    CommitRequestData* request) {
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
  request->specifics_hash = metadata_.specifics_hash();
  commit_requested_sequence_number_ = metadata_.sequence_number();
}

void ProcessorEntityTracker::ReceiveCommitResponse(
    const CommitResponseData& data) {
  DCHECK_EQ(metadata_.client_tag_hash(), data.client_tag_hash);
  DCHECK_GT(data.sequence_number, metadata_.acked_sequence_number());
  DCHECK_GT(data.response_version, metadata_.server_version());

  // The server can assign us a new ID in a commit response.
  metadata_.set_server_id(data.id);
  metadata_.set_acked_sequence_number(data.sequence_number);
  metadata_.set_server_version(data.response_version);
  if (!IsUnsynced()) {
    // Clear pending commit data if there hasn't been another commit request
    // since the one that is currently getting acked.
    commit_data_.reset();
    metadata_.clear_base_specifics_hash();
  } else {
    metadata_.set_base_specifics_hash(data.specifics_hash);
  }
}

void ProcessorEntityTracker::ClearTransientSyncState() {
  // If we have any unacknowledged commit requests outstanding, they've been
  // dropped and we should forget about them.
  commit_requested_sequence_number_ = metadata_.acked_sequence_number();
}

void ProcessorEntityTracker::IncrementSequenceNumber() {
  DCHECK(metadata_.has_sequence_number());
  if (!IsUnsynced()) {
    // Update the base specifics hash if this entity wasn't already out of sync.
    metadata_.set_base_specifics_hash(metadata_.specifics_hash());
  }
  metadata_.set_sequence_number(metadata_.sequence_number() + 1);
}

bool ProcessorEntityTracker::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_.is_deleted());
  std::string hash;
  HashSpecifics(specifics, &hash);
  return hash == metadata_.specifics_hash();
}

void ProcessorEntityTracker::UpdateSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.ByteSize() > 0) {
    HashSpecifics(specifics, metadata_.mutable_specifics_hash());
  } else {
    metadata_.clear_specifics_hash();
  }
}

}  // namespace syncer_v2
