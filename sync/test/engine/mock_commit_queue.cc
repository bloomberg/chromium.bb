// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_commit_queue.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"

namespace syncer_v2 {

MockCommitQueue::MockCommitQueue() {
}

MockCommitQueue::~MockCommitQueue() {
}

void MockCommitQueue::EnqueueForCommit(
    const CommitRequestDataList& list) {
  commit_request_lists_.push_back(list);
}

size_t MockCommitQueue::GetNumCommitRequestLists() const {
  return commit_request_lists_.size();
}

CommitRequestDataList MockCommitQueue::GetNthCommitRequestList(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitRequestLists());
  return commit_request_lists_[n];
}

bool MockCommitQueue::HasCommitRequestForTagHash(
    const std::string& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           commit_request_lists_.rbegin();
       lists_it != commit_request_lists_.rend(); ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end(); ++it) {
      if (it->entity->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }

  return false;
}

CommitRequestData MockCommitQueue::GetLatestCommitRequestForTagHash(
    const std::string& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           commit_request_lists_.rbegin();
       lists_it != commit_request_lists_.rend(); ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end(); ++it) {
      if (it->entity->client_tag_hash == tag_hash) {
        return *it;
      }
    }
  }

  NOTREACHED() << "Could not find commit for tag hash " << tag_hash << ".";
  return CommitRequestData();
}

UpdateResponseData MockCommitQueue::UpdateFromServer(
    int64_t version_offset,
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  // Overwrite the existing server version if this is the new highest version.
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.specifics = specifics;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.modification_time =
      data.creation_time + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = specifics.preference().name();

  UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = version;
  response_data.encryption_key_name = server_encryption_key_name_;

  return response_data;
}

UpdateResponseData MockCommitQueue::TombstoneFromServer(
    int64_t version_offset,
    const std::string& tag_hash) {
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

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
  response_data.encryption_key_name = server_encryption_key_name_;

  return response_data;
}

CommitResponseData MockCommitQueue::SuccessfulCommitResponse(
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

  // Increment the server version on successful commit.
  int64_t version = GetServerVersion(client_tag_hash);
  version++;
  SetServerVersion(client_tag_hash, version);

  response_data.response_version = version;

  return response_data;
}

void MockCommitQueue::SetServerEncryptionKey(
    const std::string& key_name) {
  server_encryption_key_name_ = key_name;
}

std::string MockCommitQueue::GenerateId(const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

int64_t MockCommitQueue::GetServerVersion(const std::string& tag_hash) {
  std::map<const std::string, int64_t>::const_iterator it;
  it = server_versions_.find(tag_hash);
  if (it == server_versions_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

void MockCommitQueue::SetServerVersion(const std::string& tag_hash,
                                       int64_t version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer_v2
