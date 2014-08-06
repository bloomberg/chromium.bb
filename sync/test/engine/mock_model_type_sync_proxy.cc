// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_model_type_sync_proxy.h"

#include "base/bind.h"

namespace syncer {

MockModelTypeSyncProxy::MockModelTypeSyncProxy() : is_synchronous_(true) {
}

MockModelTypeSyncProxy::~MockModelTypeSyncProxy() {
}

void MockModelTypeSyncProxy::OnCommitCompleted(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  base::Closure task =
      base::Bind(&MockModelTypeSyncProxy::OnCommitCompletedImpl,
                 base::Unretained(this),
                 type_state,
                 response_list);
  pending_tasks_.push_back(task);
  if (is_synchronous_)
    RunQueuedTasks();
}

void MockModelTypeSyncProxy::OnUpdateReceived(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list,
    const UpdateResponseDataList& pending_updates) {
  base::Closure task = base::Bind(&MockModelTypeSyncProxy::OnUpdateReceivedImpl,
                                  base::Unretained(this),
                                  type_state,
                                  response_list,
                                  pending_updates);
  pending_tasks_.push_back(task);
  if (is_synchronous_)
    RunQueuedTasks();
}

void MockModelTypeSyncProxy::SetSynchronousExecution(bool is_synchronous) {
  is_synchronous_ = is_synchronous;
}

void MockModelTypeSyncProxy::RunQueuedTasks() {
  for (std::vector<base::Closure>::iterator it = pending_tasks_.begin();
       it != pending_tasks_.end();
       ++it) {
    it->Run();
  }
  pending_tasks_.clear();
}

CommitRequestData MockModelTypeSyncProxy::CommitRequest(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  const int64 base_version = GetBaseVersion(tag_hash);

  CommitRequestData data;

  if (HasServerAssignedId(tag_hash)) {
    data.id = GetServerAssignedId(tag_hash);
  }

  data.client_tag_hash = tag_hash;
  data.sequence_number = GetNextSequenceNumber(tag_hash);
  data.deleted = false;
  data.specifics = specifics;
  data.base_version = base_version;

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(base_version);
  data.non_unique_name = "Name: " + tag_hash;

  return data;
}

CommitRequestData MockModelTypeSyncProxy::DeleteRequest(
    const std::string& tag_hash) {
  const int64 base_version = GetBaseVersion(tag_hash);
  CommitRequestData data;

  if (HasServerAssignedId(tag_hash)) {
    data.id = GetServerAssignedId(tag_hash);
  }

  data.client_tag_hash = tag_hash;
  data.sequence_number = GetNextSequenceNumber(tag_hash);
  data.base_version = base_version;
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(base_version);
  data.deleted = true;

  // These fields have little or no effect on behavior.  We set them anyway to
  // make the test more realistic.
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.non_unique_name = "Name deleted";

  return data;
}

size_t MockModelTypeSyncProxy::GetNumUpdateResponses() const {
  return received_update_responses_.size();
}

UpdateResponseDataList MockModelTypeSyncProxy::GetNthUpdateResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  return received_update_responses_[n];
}

UpdateResponseDataList MockModelTypeSyncProxy::GetNthPendingUpdates(
    size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  return received_pending_updates_[n];
}

DataTypeState MockModelTypeSyncProxy::GetNthTypeStateReceivedInUpdateResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  return type_states_received_on_update_[n];
}

size_t MockModelTypeSyncProxy::GetNumCommitResponses() const {
  return received_commit_responses_.size();
}

CommitResponseDataList MockModelTypeSyncProxy::GetNthCommitResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return received_commit_responses_[n];
}

DataTypeState MockModelTypeSyncProxy::GetNthTypeStateReceivedInCommitResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return type_states_received_on_commit_[n];
}

bool MockModelTypeSyncProxy::HasUpdateResponse(
    const std::string& tag_hash) const {
  std::map<const std::string, UpdateResponseData>::const_iterator it =
      update_response_items_.find(tag_hash);
  return it != update_response_items_.end();
}

UpdateResponseData MockModelTypeSyncProxy::GetUpdateResponse(
    const std::string& tag_hash) const {
  DCHECK(HasUpdateResponse(tag_hash));
  std::map<const std::string, UpdateResponseData>::const_iterator it =
      update_response_items_.find(tag_hash);
  return it->second;
}

bool MockModelTypeSyncProxy::HasCommitResponse(
    const std::string& tag_hash) const {
  std::map<const std::string, CommitResponseData>::const_iterator it =
      commit_response_items_.find(tag_hash);
  return it != commit_response_items_.end();
}

CommitResponseData MockModelTypeSyncProxy::GetCommitResponse(
    const std::string& tag_hash) const {
  DCHECK(HasCommitResponse(tag_hash));
  std::map<const std::string, CommitResponseData>::const_iterator it =
      commit_response_items_.find(tag_hash);
  return it->second;
}

void MockModelTypeSyncProxy::OnCommitCompletedImpl(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  received_commit_responses_.push_back(response_list);
  type_states_received_on_commit_.push_back(type_state);
  for (CommitResponseDataList::const_iterator it = response_list.begin();
       it != response_list.end();
       ++it) {
    commit_response_items_.insert(std::make_pair(it->client_tag_hash, *it));

    // Server wins.  Set the model's base version.
    SetBaseVersion(it->client_tag_hash, it->response_version);
    SetServerAssignedId(it->client_tag_hash, it->id);
  }
}

void MockModelTypeSyncProxy::OnUpdateReceivedImpl(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list,
    const UpdateResponseDataList& pending_updates) {
  received_update_responses_.push_back(response_list);
  received_pending_updates_.push_back(pending_updates);
  type_states_received_on_update_.push_back(type_state);
  for (UpdateResponseDataList::const_iterator it = response_list.begin();
       it != response_list.end();
       ++it) {
    update_response_items_.insert(std::make_pair(it->client_tag_hash, *it));

    // Server wins.  Set the model's base version.
    SetBaseVersion(it->client_tag_hash, it->response_version);
    SetServerAssignedId(it->client_tag_hash, it->id);
  }
}

// Fetches the sequence number as of the most recent update request.
int64 MockModelTypeSyncProxy::GetCurrentSequenceNumber(
    const std::string& tag_hash) const {
  std::map<const std::string, int64>::const_iterator it =
      sequence_numbers_.find(tag_hash);
  if (it == sequence_numbers_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

// The model thread should be sending us items with strictly increasing
// sequence numbers.  Here's where we emulate that behavior.
int64 MockModelTypeSyncProxy::GetNextSequenceNumber(
    const std::string& tag_hash) {
  int64 sequence_number = GetCurrentSequenceNumber(tag_hash);
  sequence_number++;
  sequence_numbers_[tag_hash] = sequence_number;
  return sequence_number;
}

int64 MockModelTypeSyncProxy::GetBaseVersion(
    const std::string& tag_hash) const {
  std::map<const std::string, int64>::const_iterator it =
      base_versions_.find(tag_hash);
  if (it == base_versions_.end()) {
    return kUncommittedVersion;
  } else {
    return it->second;
  }
}

void MockModelTypeSyncProxy::SetBaseVersion(const std::string& tag_hash,
                                            int64 version) {
  base_versions_[tag_hash] = version;
}

bool MockModelTypeSyncProxy::HasServerAssignedId(
    const std::string& tag_hash) const {
  return assigned_ids_.find(tag_hash) != assigned_ids_.end();
}

const std::string& MockModelTypeSyncProxy::GetServerAssignedId(
    const std::string& tag_hash) const {
  DCHECK(HasServerAssignedId(tag_hash));
  return assigned_ids_.find(tag_hash)->second;
}

void MockModelTypeSyncProxy::SetServerAssignedId(const std::string& tag_hash,
                                                 const std::string& id) {
  assigned_ids_[tag_hash] = id;
}

}  // namespace syncer
