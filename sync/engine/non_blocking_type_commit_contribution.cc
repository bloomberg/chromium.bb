// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_commit_contribution.h"

#include "sync/engine/model_type_sync_worker_impl.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

NonBlockingTypeCommitContribution::NonBlockingTypeCommitContribution(
    const sync_pb::DataTypeContext& context,
    const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
    const std::vector<int64>& sequence_numbers,
    ModelTypeSyncWorkerImpl* worker)
    : worker_(worker),
      context_(context),
      entities_(entities),
      sequence_numbers_(sequence_numbers),
      cleaned_up_(false) {
}

NonBlockingTypeCommitContribution::~NonBlockingTypeCommitContribution() {
  DCHECK(cleaned_up_);
}

void NonBlockingTypeCommitContribution::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();

  std::copy(entities_.begin(),
            entities_.end(),
            RepeatedPtrFieldBackInserter(commit_message->mutable_entries()));
  if (!context_.context().empty())
    commit_message->add_client_contexts()->CopyFrom(context_);
}

SyncerError NonBlockingTypeCommitContribution::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    sessions::StatusController* status) {
  const sync_pb::CommitResponse& commit_response = response.commit();

  bool transient_error = false;
  bool commit_conflict = false;
  bool unknown_error = false;

  CommitResponseDataList response_list;

  for (size_t i = 0; i < sequence_numbers_.size(); ++i) {
    const sync_pb::CommitResponse_EntryResponse& entry_response =
        commit_response.entryresponse(entries_start_index_ + i);

    switch (entry_response.response_type()) {
      case sync_pb::CommitResponse::INVALID_MESSAGE:
        LOG(ERROR) << "Server reports commit message is invalid.";
        DLOG(ERROR) << "Message was: " << SyncEntityToValue(entities_.Get(i),
                                                            false);
        unknown_error = true;
        break;
      case sync_pb::CommitResponse::CONFLICT:
        DVLOG(1) << "Server reports conflict for commit message.";
        DVLOG(1) << "Message was: " << SyncEntityToValue(entities_.Get(i),
                                                         false);
        commit_conflict = true;
        break;
      case sync_pb::CommitResponse::SUCCESS: {
        CommitResponseData response_data;
        response_data.id = entry_response.id_string();
        response_data.client_tag_hash =
            entities_.Get(i).client_defined_unique_tag();
        response_data.sequence_number = sequence_numbers_[i];
        response_data.response_version = entry_response.version();
        response_list.push_back(response_data);
        break;
      }
      case sync_pb::CommitResponse::OVER_QUOTA:
      case sync_pb::CommitResponse::RETRY:
      case sync_pb::CommitResponse::TRANSIENT_ERROR:
        DLOG(WARNING) << "Entity commit blocked by transient error.";
        transient_error = true;
        break;
      default:
        LOG(ERROR) << "Bad return from ProcessSingleCommitResponse.";
        unknown_error = true;
    }
  }

  // Send whatever successful responses we did get back to our parent.
  // It's the schedulers job to handle the failures.
  worker_->OnCommitResponse(response_list);

  // Let the scheduler know about the failures.
  if (unknown_error) {
    return SERVER_RETURN_UNKNOWN_ERROR;
  } else if (transient_error) {
    return SERVER_RETURN_TRANSIENT_ERROR;
  } else if (commit_conflict) {
    return SERVER_RETURN_CONFLICT;
  } else {
    return SYNCER_OK;
  }
}

void NonBlockingTypeCommitContribution::CleanUp() {
  cleaned_up_ = true;

  // We could inform our parent NonBlockingCommitContributor that a commit is
  // no longer in progress.  The current implementation doesn't really care
  // either way, so we don't bother sending the signal.
}

size_t NonBlockingTypeCommitContribution::GetNumEntries() const {
  return sequence_numbers_.size();
}

}  // namespace syncer
