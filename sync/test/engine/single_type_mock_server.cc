// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/single_type_mock_server.h"

#include "sync/util/time.h"

using google::protobuf::RepeatedPtrField;

namespace syncer {

SingleTypeMockServer::SingleTypeMockServer(syncer::ModelType type)
    : type_(type), type_root_id_(ModelTypeToRootTag(type)) {
}

SingleTypeMockServer::~SingleTypeMockServer() {
}

sync_pb::SyncEntity SingleTypeMockServer::TypeRootUpdate() {
  sync_pb::SyncEntity entity;

  entity.set_id_string(type_root_id_);
  entity.set_parent_id_string("r");
  entity.set_version(1000);
  entity.set_ctime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_mtime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_server_defined_unique_tag(ModelTypeToRootTag(type_));
  entity.set_deleted(false);
  AddDefaultFieldValue(type_, entity.mutable_specifics());

  return entity;
}

sync_pb::SyncEntity SingleTypeMockServer::UpdateFromServer(
    int64 version_offset,
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  sync_pb::SyncEntity entity;

  entity.set_id_string(GenerateId(tag_hash));
  entity.set_parent_id_string(type_root_id_);
  entity.set_version(version);
  entity.set_client_defined_unique_tag(tag_hash);
  entity.set_deleted(false);
  entity.mutable_specifics()->CopyFrom(specifics);

  // Unimportant fields, set for completeness only.
  base::Time ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time mtime = ctime + base::TimeDelta::FromSeconds(version);
  entity.set_ctime(TimeToProtoTime(ctime));
  entity.set_mtime(TimeToProtoTime(mtime));
  entity.set_name("Name: " + tag_hash);

  return entity;
}

sync_pb::SyncEntity SingleTypeMockServer::TombstoneFromServer(
    int64 version_offset,
    const std::string& tag_hash) {
  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  sync_pb::SyncEntity entity;

  entity.set_id_string(GenerateId(tag_hash));
  entity.set_parent_id_string(type_root_id_);
  entity.set_version(version);
  entity.set_client_defined_unique_tag(tag_hash);
  entity.set_deleted(false);
  AddDefaultFieldValue(type_, entity.mutable_specifics());

  // Unimportant fields, set for completeness only.
  base::Time ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time mtime = ctime + base::TimeDelta::FromSeconds(version);
  entity.set_ctime(TimeToProtoTime(ctime));
  entity.set_mtime(TimeToProtoTime(mtime));
  entity.set_name("Tombstone");

  return entity;
}

sync_pb::ClientToServerResponse SingleTypeMockServer::DoSuccessfulCommit(
    const sync_pb::ClientToServerMessage& message) {
  commit_messages_.push_back(message);

  sync_pb::ClientToServerResponse response;
  sync_pb::CommitResponse* commit_response = response.mutable_commit();

  const RepeatedPtrField<sync_pb::SyncEntity>& entries =
      message.commit().entries();
  for (RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it =
           entries.begin();
       it != entries.end();
       ++it) {
    const std::string tag_hash = it->client_defined_unique_tag();

    committed_items_[tag_hash] = *it;

    // Every commit increments the version number.
    int64 version = GetServerVersion(tag_hash);
    version++;
    SetServerVersion(tag_hash, version);

    sync_pb::CommitResponse_EntryResponse* entryresponse =
        commit_response->add_entryresponse();
    entryresponse->set_response_type(sync_pb::CommitResponse::SUCCESS);
    entryresponse->set_id_string(GenerateId(tag_hash));
    entryresponse->set_parent_id_string(it->parent_id_string());
    entryresponse->set_version(version);
    entryresponse->set_name(it->name());
    entryresponse->set_mtime(it->mtime());
  }

  return response;
}

size_t SingleTypeMockServer::GetNumCommitMessages() const {
  return commit_messages_.size();
}

sync_pb::ClientToServerMessage SingleTypeMockServer::GetNthCommitMessage(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitMessages());
  return commit_messages_[n];
}

bool SingleTypeMockServer::HasCommitEntity(const std::string& tag_hash) const {
  return committed_items_.find(tag_hash) != committed_items_.end();
}

sync_pb::SyncEntity SingleTypeMockServer::GetLastCommittedEntity(
    const std::string& tag_hash) const {
  DCHECK(HasCommitEntity(tag_hash));
  return committed_items_.find(tag_hash)->second;
}

sync_pb::DataTypeProgressMarker SingleTypeMockServer::GetProgress() const {
  sync_pb::DataTypeProgressMarker progress;
  progress.set_data_type_id(type_);
  progress.set_token("non_null_progress_token");
  return progress;
}

sync_pb::DataTypeContext SingleTypeMockServer::GetContext() const {
  return sync_pb::DataTypeContext();
}

std::string SingleTypeMockServer::GenerateId(const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

int64 SingleTypeMockServer::GetServerVersion(
    const std::string& tag_hash) const {
  std::map<const std::string, int64>::const_iterator it;
  it = server_versions_.find(tag_hash);
  // Server versions do not necessarily start at 1 or 0.
  if (it == server_versions_.end()) {
    return 2048;
  } else {
    return it->second;
  }
}

void SingleTypeMockServer::SetServerVersion(const std::string& tag_hash,
                                            int64 version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer
