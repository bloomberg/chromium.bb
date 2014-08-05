// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_sync_worker_impl.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sync/engine/commit_contribution.h"
#include "sync/engine/entity_tracker.h"
#include "sync/engine/model_type_sync_proxy.h"
#include "sync/engine/non_blocking_type_commit_contribution.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

namespace syncer {

ModelTypeSyncWorkerImpl::ModelTypeSyncWorkerImpl(
    ModelType type,
    const DataTypeState& initial_state,
    NudgeHandler* nudge_handler,
    scoped_ptr<ModelTypeSyncProxy> type_sync_proxy)
    : type_(type),
      data_type_state_(initial_state),
      type_sync_proxy_(type_sync_proxy.Pass()),
      nudge_handler_(nudge_handler),
      entities_deleter_(&entities_),
      weak_ptr_factory_(this) {
  // Request an initial sync if it hasn't been completed yet.
  if (!data_type_state_.initial_sync_done) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }
}

ModelTypeSyncWorkerImpl::~ModelTypeSyncWorkerImpl() {
}

ModelType ModelTypeSyncWorkerImpl::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

// UpdateHandler implementation.
void ModelTypeSyncWorkerImpl::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  DCHECK(CalledOnValidThread());
  progress_marker->CopyFrom(data_type_state_.progress_marker);
}

void ModelTypeSyncWorkerImpl::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  DCHECK(CalledOnValidThread());
  context->CopyFrom(data_type_state_.type_context);
}

SyncerError ModelTypeSyncWorkerImpl::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());

  // TODO(rlarocque): Handle data type context conflicts.
  data_type_state_.type_context = mutated_context;
  data_type_state_.progress_marker = progress_marker;

  UpdateResponseDataList response_datas;

  for (SyncEntityList::const_iterator update_it = applicable_updates.begin();
       update_it != applicable_updates.end();
       ++update_it) {
    const sync_pb::SyncEntity* update_entity = *update_it;
    if (!update_entity->server_defined_unique_tag().empty()) {
      // We can't commit an item unless we know its parent ID.  This is where
      // we learn that ID and remember it forever.
      DCHECK_EQ(ModelTypeToRootTag(type_),
                update_entity->server_defined_unique_tag());
      if (!data_type_state_.type_root_id.empty()) {
        DCHECK_EQ(data_type_state_.type_root_id, update_entity->id_string());
      }
      data_type_state_.type_root_id = update_entity->id_string();
    } else {
      // Normal updates are handled here.
      const std::string& client_tag_hash =
          update_entity->client_defined_unique_tag();
      DCHECK(!client_tag_hash.empty());
      EntityMap::const_iterator map_it = entities_.find(client_tag_hash);
      if (map_it == entities_.end()) {
        EntityTracker* entity =
            EntityTracker::FromServerUpdate(update_entity->id_string(),
                                            client_tag_hash,
                                            update_entity->version());
        entities_.insert(std::make_pair(client_tag_hash, entity));
      } else {
        EntityTracker* entity = map_it->second;
        entity->ReceiveUpdate(update_entity->version());
      }

      // Prepare the message for the model thread.
      UpdateResponseData response_data;
      response_data.id = update_entity->id_string();
      response_data.client_tag_hash = client_tag_hash;
      response_data.response_version = update_entity->version();
      response_data.ctime = ProtoTimeToTime(update_entity->ctime());
      response_data.mtime = ProtoTimeToTime(update_entity->mtime());
      response_data.non_unique_name = update_entity->name();
      response_data.deleted = update_entity->deleted();
      response_data.specifics = update_entity->specifics();

      response_datas.push_back(response_data);
    }
  }

  // Forward these updates to the model thread so it can do the rest.
  type_sync_proxy_->OnUpdateReceived(data_type_state_, response_datas);

  return SYNCER_OK;
}

void ModelTypeSyncWorkerImpl::ApplyUpdates(sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // This function is called only when we've finished a download cycle, ie. we
  // got a response with changes_remaining == 0.  If this is our first download
  // cycle, we should update our state so the ModelTypeSyncProxy knows that
  // it's safe to commit items now.
  if (!data_type_state_.initial_sync_done) {
    data_type_state_.initial_sync_done = true;

    UpdateResponseDataList empty_update_list;
    type_sync_proxy_->OnUpdateReceived(data_type_state_, empty_update_list);
  }
}

void ModelTypeSyncWorkerImpl::PassiveApplyUpdates(
    sessions::StatusController* status) {
  NOTREACHED()
      << "Non-blocking types should never apply updates on sync thread.  "
      << "ModelType is: " << ModelTypeToString(type_);
}

void ModelTypeSyncWorkerImpl::EnqueueForCommit(
    const CommitRequestDataList& list) {
  DCHECK(CalledOnValidThread());

  DCHECK(CanCommitItems())
      << "Asked to commit items before type was initialized.  "
      << "ModelType is: " << ModelTypeToString(type_);

  for (CommitRequestDataList::const_iterator it = list.begin();
       it != list.end();
       ++it) {
    StorePendingCommit(*it);
  }
}

// CommitContributor implementation.
scoped_ptr<CommitContribution> ModelTypeSyncWorkerImpl::GetContribution(
    size_t max_entries) {
  DCHECK(CalledOnValidThread());

  size_t space_remaining = max_entries;
  std::vector<int64> sequence_numbers;
  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> commit_entities;

  if (!CanCommitItems())
    return scoped_ptr<CommitContribution>();

  // TODO(rlarocque): Avoid iterating here.
  for (EntityMap::const_iterator it = entities_.begin();
       it != entities_.end() && space_remaining > 0;
       ++it) {
    EntityTracker* entity = it->second;
    if (entity->IsCommitPending()) {
      sync_pb::SyncEntity* commit_entity = commit_entities.Add();
      int64 sequence_number = -1;

      entity->PrepareCommitProto(commit_entity, &sequence_number);
      HelpInitializeCommitEntity(commit_entity);
      sequence_numbers.push_back(sequence_number);

      space_remaining--;
    }
  }

  if (commit_entities.size() == 0)
    return scoped_ptr<CommitContribution>();

  return scoped_ptr<CommitContribution>(new NonBlockingTypeCommitContribution(
      data_type_state_.type_context, commit_entities, sequence_numbers, this));
}

void ModelTypeSyncWorkerImpl::StorePendingCommit(
    const CommitRequestData& request) {
  if (!request.deleted) {
    DCHECK_EQ(type_, GetModelTypeFromSpecifics(request.specifics));
  }

  EntityMap::iterator map_it = entities_.find(request.client_tag_hash);
  if (map_it == entities_.end()) {
    EntityTracker* entity =
        EntityTracker::FromCommitRequest(request.id,
                                         request.client_tag_hash,
                                         request.sequence_number,
                                         request.base_version,
                                         request.ctime,
                                         request.mtime,
                                         request.non_unique_name,
                                         request.deleted,
                                         request.specifics);
    entities_.insert(std::make_pair(request.client_tag_hash, entity));
  } else {
    EntityTracker* entity = map_it->second;
    entity->RequestCommit(request.id,
                          request.client_tag_hash,
                          request.sequence_number,
                          request.base_version,
                          request.ctime,
                          request.mtime,
                          request.non_unique_name,
                          request.deleted,
                          request.specifics);
  }

  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

void ModelTypeSyncWorkerImpl::OnCommitResponse(
    const CommitResponseDataList& response_list) {
  for (CommitResponseDataList::const_iterator response_it =
           response_list.begin();
       response_it != response_list.end();
       ++response_it) {
    const std::string client_tag_hash = response_it->client_tag_hash;
    EntityMap::iterator map_it = entities_.find(client_tag_hash);

    // There's no way we could have committed an entry we know nothing about.
    if (map_it == entities_.end()) {
      NOTREACHED() << "Received commit response for item unknown to us."
                   << " Model type: " << ModelTypeToString(type_)
                   << " ID: " << response_it->id;
      continue;
    }

    EntityTracker* entity = map_it->second;
    entity->ReceiveCommitResponse(response_it->id,
                                  response_it->response_version,
                                  response_it->sequence_number);
  }

  // Send the responses back to the model thread.  It needs to know which
  // items have been successfully committed so it can save that information in
  // permanent storage.
  type_sync_proxy_->OnCommitCompleted(data_type_state_, response_list);
}

base::WeakPtr<ModelTypeSyncWorkerImpl> ModelTypeSyncWorkerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ModelTypeSyncWorkerImpl::CanCommitItems() const {
  // We can't commit anything until we know the type's parent node.
  // We'll get it in the first update response.
  return !data_type_state_.type_root_id.empty() &&
         data_type_state_.initial_sync_done;
}

void ModelTypeSyncWorkerImpl::HelpInitializeCommitEntity(
    sync_pb::SyncEntity* sync_entity) {
  // Initial commits need our help to generate a client ID.
  if (!sync_entity->has_id_string()) {
    DCHECK_EQ(kUncommittedVersion, sync_entity->version());
    const int64 id = data_type_state_.next_client_id++;
    sync_entity->set_id_string(
        base::StringPrintf("%s-%" PRId64, ModelTypeToString(type_), id));
  }

  // Always include enough specifics to identify the type.  Do this even in
  // deletion requests, where the specifics are otherwise invalid.
  if (!sync_entity->has_specifics()) {
    AddDefaultFieldValue(type_, sync_entity->mutable_specifics());
  }

  // We're always responsible for the parent ID.
  sync_entity->set_parent_id_string(data_type_state_.type_root_id);
}

}  // namespace syncer
