// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/commit.h"

#include "base/debug/trace_event.h"
#include "sync/engine/commit_contribution.h"
#include "sync/engine/commit_processor.h"
#include "sync/engine/commit_util.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/events/commit_request_event.h"
#include "sync/internal_api/public/events/commit_response_event.h"
#include "sync/sessions/sync_session.h"

namespace syncer {

Commit::Commit(
    const std::map<ModelType, CommitContribution*>& contributions,
    const sync_pb::ClientToServerMessage& message,
    ExtensionsActivity::Records extensions_activity_buffer)
  : contributions_(contributions),
    deleter_(&contributions_),
    message_(message),
    extensions_activity_buffer_(extensions_activity_buffer),
    cleaned_up_(false) {
}

Commit::~Commit() {
  DCHECK(cleaned_up_);
}

Commit* Commit::Init(
    ModelTypeSet requested_types,
    ModelTypeSet enabled_types,
    size_t max_entries,
    const std::string& account_name,
    const std::string& cache_guid,
    CommitProcessor* commit_processor,
    ExtensionsActivity* extensions_activity) {
  // Gather per-type contributions.
  ContributionMap contributions;
  commit_processor->GatherCommitContributions(
      requested_types,
      max_entries,
      &contributions);

  // Give up if no one had anything to commit.
  if (contributions.empty())
    return NULL;

  sync_pb::ClientToServerMessage message;
  message.set_message_contents(sync_pb::ClientToServerMessage::COMMIT);
  message.set_share(account_name);

  sync_pb::CommitMessage* commit_message = message.mutable_commit();
  commit_message->set_cache_guid(cache_guid);

  // Set extensions activity if bookmark commits are present.
  ExtensionsActivity::Records extensions_activity_buffer;
  ContributionMap::iterator it = contributions.find(syncer::BOOKMARKS);
  if (it != contributions.end() && it->second->GetNumEntries() != 0) {
    commit_util::AddExtensionsActivityToMessage(
        extensions_activity,
        &extensions_activity_buffer,
        commit_message);
  }

  // Set the client config params.
  commit_util::AddClientConfigParamsToMessage(
      enabled_types,
      commit_message);

  // Finally, serialize all our contributions.
  for (std::map<ModelType, CommitContribution*>::iterator it =
           contributions.begin(); it != contributions.end(); ++it) {
    it->second->AddToCommitMessage(&message);
  }

  // If we made it this far, then we've successfully prepared a commit message.
  return new Commit(contributions, message, extensions_activity_buffer);
}

SyncerError Commit::PostAndProcessResponse(
    sessions::SyncSession* session,
    sessions::StatusController* status,
    ExtensionsActivity* extensions_activity) {
  ModelTypeSet request_types;
  for (ContributionMap::const_iterator it = contributions_.begin();
       it != contributions_.end(); ++it) {
    request_types.Put(it->first);
  }
  session->mutable_status_controller()->set_commit_request_types(request_types);

  if (session->context()->debug_info_getter()) {
    sync_pb::DebugInfo* debug_info = message_.mutable_debug_info();
    session->context()->debug_info_getter()->GetDebugInfo(debug_info);
  }

  DVLOG(1) << "Sending commit message.";

  CommitRequestEvent request_event(
      base::Time::Now(),
      message_.commit().entries_size(),
      request_types,
      message_);
  session->SendProtocolEvent(request_event);

  TRACE_EVENT_BEGIN0("sync", "PostCommit");
  const SyncerError post_result = SyncerProtoUtil::PostClientToServerMessage(
      &message_, &response_, session);
  TRACE_EVENT_END0("sync", "PostCommit");

  // TODO(rlarocque): Use result that includes errors captured later?
  CommitResponseEvent response_event(
      base::Time::Now(),
      post_result,
      response_);
  session->SendProtocolEvent(response_event);

  if (post_result != SYNCER_OK) {
    LOG(WARNING) << "Post commit failed";
    return post_result;
  }

  if (!response_.has_commit()) {
    LOG(WARNING) << "Commit response has no commit body!";
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }

  size_t message_entries = message_.commit().entries_size();
  size_t response_entries = response_.commit().entryresponse_size();
  if (message_entries != response_entries) {
    LOG(ERROR)
       << "Commit response has wrong number of entries! "
       << "Expected: " << message_entries << ", "
       << "Got: " << response_entries;
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }

  if (session->context()->debug_info_getter()) {
    // Clear debug info now that we have successfully sent it to the server.
    DVLOG(1) << "Clearing client debug info.";
    session->context()->debug_info_getter()->ClearDebugInfo();
  }

  // Let the contributors process the responses to each of their requests.
  SyncerError processing_result = SYNCER_OK;
  for (std::map<ModelType, CommitContribution*>::iterator it =
       contributions_.begin(); it != contributions_.end(); ++it) {
    TRACE_EVENT1("sync", "ProcessCommitResponse",
                 "type", ModelTypeToString(it->first));
    SyncerError type_result =
        it->second->ProcessCommitResponse(response_, status);
    if (processing_result == SYNCER_OK && type_result != SYNCER_OK) {
      processing_result = type_result;
    }
  }

  // Handle bookmarks' special extensions activity stats.
  if (session->status_controller().
          model_neutral_state().num_successful_bookmark_commits == 0) {
    extensions_activity->PutRecords(extensions_activity_buffer_);
  }

  return processing_result;
}

void Commit::CleanUp() {
  for (ContributionMap::iterator it = contributions_.begin();
       it != contributions_.end(); ++it) {
    it->second->CleanUp();
  }
  cleaned_up_ = true;
}

}  // namespace syncer
