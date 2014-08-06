// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_model_type_sync_worker.h"

#include "base/logging.h"

namespace syncer {

MockModelTypeSyncWorker::MockModelTypeSyncWorker() {
}

MockModelTypeSyncWorker::~MockModelTypeSyncWorker() {
}

void MockModelTypeSyncWorker::EnqueueForCommit(
    const CommitRequestDataList& list) {
  commit_request_lists_.push_back(list);
}

size_t MockModelTypeSyncWorker::GetNumCommitRequestLists() const {
  return commit_request_lists_.size();
}

CommitRequestDataList MockModelTypeSyncWorker::GetNthCommitRequestList(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitRequestLists());
  return commit_request_lists_[n];
}

bool MockModelTypeSyncWorker::HasCommitRequestForTagHash(
    const std::string& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           commit_request_lists_.rbegin();
       lists_it != commit_request_lists_.rend();
       ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end();
         ++it) {
      if (it->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }

  return false;
}

CommitRequestData MockModelTypeSyncWorker::GetLatestCommitRequestForTagHash(
    const std::string& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           commit_request_lists_.rbegin();
       lists_it != commit_request_lists_.rend();
       ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end();
         ++it) {
      if (it->client_tag_hash == tag_hash) {
        return *it;
      }
    }
  }

  NOTREACHED() << "Could not find commit for tag hash " << tag_hash << ".";
  return CommitRequestData();
}

UpdateResponseData MockModelTypeSyncWorker::UpdateFromServer(
    int64 version_offset,
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  // Overwrite the existing server version if this is the new highest version.
  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  UpdateResponseData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.response_version = version;
  data.deleted = false;
  data.specifics = specifics;

  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = specifics.preference().name();

  data.encryption_key_name = server_encryption_key_name_;

  return data;
}

UpdateResponseData MockModelTypeSyncWorker::TombstoneFromServer(
    int64 version_offset,
    const std::string& tag_hash) {
  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  UpdateResponseData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.response_version = version;
  data.deleted = true;

  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = "Name Non Unique";

  data.encryption_key_name = server_encryption_key_name_;

  return data;
}

CommitResponseData MockModelTypeSyncWorker::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  const std::string& client_tag_hash = request_data.client_tag_hash;

  CommitResponseData response_data;

  if (request_data.base_version == 0) {
    // Server assigns new ID to newly committed items.
    DCHECK(request_data.id.empty());
    response_data.id = request_data.id;
  } else {
    // Otherwise we reuse the ID from the request.
    response_data.id = GenerateId(client_tag_hash);
  }

  response_data.client_tag_hash = client_tag_hash;
  response_data.sequence_number = request_data.sequence_number;

  // Increment the server version on successful commit.
  int64 version = GetServerVersion(client_tag_hash);
  version++;
  SetServerVersion(client_tag_hash, version);

  response_data.response_version = version;

  return response_data;
}

void MockModelTypeSyncWorker::SetServerEncryptionKey(
    const std::string& key_name) {
  server_encryption_key_name_ = key_name;
}

std::string MockModelTypeSyncWorker::GenerateId(const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

int64 MockModelTypeSyncWorker::GetServerVersion(const std::string& tag_hash) {
  std::map<const std::string, int64>::const_iterator it;
  it = server_versions_.find(tag_hash);
  if (it == server_versions_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

void MockModelTypeSyncWorker::SetServerVersion(const std::string& tag_hash,
                                               int64 version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer
