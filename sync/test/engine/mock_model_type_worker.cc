// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_model_type_worker.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

namespace {

std::string GenerateTagHash(const std::string& tag) {
  return syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES, tag);
}

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                           const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

}  // namespace

MockModelTypeWorker::MockModelTypeWorker(
    const sync_pb::DataTypeState& data_type_state,
    ModelTypeProcessor* processor)
    : data_type_state_(data_type_state), processor_(processor) {}

MockModelTypeWorker::~MockModelTypeWorker() {}

void MockModelTypeWorker::EnqueueForCommit(const CommitRequestDataList& list) {
  pending_commits_.push_back(list);
}

size_t MockModelTypeWorker::GetNumPendingCommits() const {
  return pending_commits_.size();
}

CommitRequestDataList MockModelTypeWorker::GetNthPendingCommit(size_t n) const {
  DCHECK_LT(n, GetNumPendingCommits());
  return pending_commits_[n];
}

bool MockModelTypeWorker::HasPendingCommitForTag(const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  for (const CommitRequestDataList& commit : pending_commits_) {
    for (const CommitRequestData& data : commit) {
      if (data.entity->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }
  return false;
}

CommitRequestData MockModelTypeWorker::GetLatestPendingCommitForTag(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (auto rev_it = pending_commits_.rbegin();
       rev_it != pending_commits_.rend(); ++rev_it) {
    for (const CommitRequestData& data : *rev_it) {
      if (data.entity->client_tag_hash == tag_hash) {
        return data;
      }
    }
  }
  NOTREACHED() << "Could not find commit for tag hash " << tag_hash << ".";
  return CommitRequestData();
}

void MockModelTypeWorker::ExpectNthPendingCommit(size_t n,
                                                 const std::string& tag,
                                                 const std::string& value) {
  const CommitRequestDataList& list = GetNthPendingCommit(n);
  ASSERT_EQ(1U, list.size());
  const EntityData& data = list[0].entity.value();
  EXPECT_EQ(GenerateTagHash(tag), data.client_tag_hash);
  EXPECT_EQ(value, data.specifics.preference().value());
}

void MockModelTypeWorker::ExpectPendingCommits(
    const std::vector<std::string>& tags) {
  EXPECT_EQ(tags.size(), GetNumPendingCommits());
  for (size_t i = 0; i < tags.size(); i++) {
    const CommitRequestDataList& commits = GetNthPendingCommit(i);
    EXPECT_EQ(1U, commits.size());
    EXPECT_EQ(GenerateTagHash(tags[i]), commits[0].entity->client_tag_hash)
        << "Hash for tag " << tags[i] << " doesn't match.";
  }
}

void MockModelTypeWorker::UpdateFromServer(const std::string& tag,
                                           const std::string& value) {
  UpdateFromServer(tag, value, 1);
}

void MockModelTypeWorker::UpdateFromServer(const std::string& tag,
                                           const std::string& value,
                                           int64_t version_offset) {
  UpdateFromServer(tag, value, version_offset,
                   data_type_state_.encryption_key_name());
}

void MockModelTypeWorker::UpdateFromServer(const std::string& tag,
                                           const std::string& value,
                                           int64_t version_offset,
                                           const std::string& ekn) {
  UpdateResponseDataList update;
  update.push_back(GenerateUpdateData(tag, value, version_offset, ekn));
  processor_->OnUpdateReceived(data_type_state_, update);
}

UpdateResponseData MockModelTypeWorker::GenerateUpdateData(
    const std::string& tag,
    const std::string& value,
    int64_t version_offset,
    const std::string& ekn) {
  const std::string tag_hash = GenerateTagHash(tag);
  // Overwrite the existing server version if this is the new highest version.
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.specifics = GenerateSpecifics(tag, value);
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.modification_time =
      data.creation_time + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = data.specifics.preference().name();

  UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = version;
  response_data.encryption_key_name = ekn;

  return response_data;
}

void MockModelTypeWorker::TombstoneFromServer(const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + 1;
  SetServerVersion(tag_hash, version);

  EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.modification_time =
      data.creation_time + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = "Name Non Unique";

  UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = version;
  response_data.encryption_key_name = data_type_state_.encryption_key_name();

  UpdateResponseDataList list;
  list.push_back(response_data);
  processor_->OnUpdateReceived(data_type_state_, list);
}

void MockModelTypeWorker::AckOnePendingCommit() {
  CommitResponseDataList list;
  for (const CommitRequestData& data : pending_commits_.front()) {
    list.push_back(SuccessfulCommitResponse(data));
  }
  pending_commits_.pop_front();
  processor_->OnCommitCompleted(data_type_state_, list);
}

CommitResponseData MockModelTypeWorker::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  const EntityData& entity = request_data.entity.value();
  const std::string& client_tag_hash = entity.client_tag_hash;

  CommitResponseData response_data;

  if (request_data.base_version == 0) {
    // Server assigns new ID to newly committed items.
    DCHECK(entity.id.empty());
    response_data.id = entity.id;
  } else {
    // Otherwise we reuse the ID from the request.
    response_data.id = GenerateId(client_tag_hash);
  }

  response_data.client_tag_hash = client_tag_hash;
  response_data.sequence_number = request_data.sequence_number;
  response_data.specifics_hash = request_data.specifics_hash;

  // Increment the server version on successful commit.
  int64_t version = GetServerVersion(client_tag_hash);
  version++;
  SetServerVersion(client_tag_hash, version);

  response_data.response_version = version;

  return response_data;
}

void MockModelTypeWorker::UpdateWithEncryptionKey(const std::string& ekn) {
  UpdateWithEncryptionKey(ekn, UpdateResponseDataList());
}

void MockModelTypeWorker::UpdateWithEncryptionKey(
    const std::string& ekn,
    const UpdateResponseDataList& update) {
  data_type_state_.set_encryption_key_name(ekn);
  processor_->OnUpdateReceived(data_type_state_, update);
}

std::string MockModelTypeWorker::GenerateId(const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

int64_t MockModelTypeWorker::GetServerVersion(const std::string& tag_hash) {
  std::map<const std::string, int64_t>::const_iterator it;
  it = server_versions_.find(tag_hash);
  if (it == server_versions_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

void MockModelTypeWorker::SetServerVersion(const std::string& tag_hash,
                                           int64_t version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer_v2
