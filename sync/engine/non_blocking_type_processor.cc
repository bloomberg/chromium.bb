// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "sync/engine/model_thread_sync_entity.h"
#include "sync/engine/non_blocking_type_processor_core_interface.h"
#include "sync/internal_api/public/sync_core_proxy.h"
#include "sync/syncable/syncable_util.h"

namespace syncer {

NonBlockingTypeProcessor::NonBlockingTypeProcessor(ModelType type)
    : type_(type),
      is_preferred_(false),
      is_connected_(false),
      entities_deleter_(&entities_),
      weak_ptr_factory_for_ui_(this),
      weak_ptr_factory_for_sync_(this) {
}

NonBlockingTypeProcessor::~NonBlockingTypeProcessor() {
}

bool NonBlockingTypeProcessor::IsPreferred() const {
  DCHECK(CalledOnValidThread());
  return is_preferred_;
}

bool NonBlockingTypeProcessor::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return is_connected_;
}

ModelType NonBlockingTypeProcessor::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

void NonBlockingTypeProcessor::Enable(
    scoped_ptr<SyncCoreProxy> sync_core_proxy) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to enable " << ModelTypeToString(type_);

  is_preferred_ = true;

  // TODO(rlarocque): At some point, this should be loaded from storage.
  data_type_state_.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(type_));
  data_type_state_.next_client_id = 0;

  sync_core_proxy_ = sync_core_proxy.Pass();
  sync_core_proxy_->ConnectTypeToCore(GetModelType(),
                                      data_type_state_,
                                      weak_ptr_factory_for_sync_.GetWeakPtr());
}

void NonBlockingTypeProcessor::Disable() {
  DCHECK(CalledOnValidThread());
  is_preferred_ = false;
  Disconnect();
}

void NonBlockingTypeProcessor::Disconnect() {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to disconnect " << ModelTypeToString(type_);
  is_connected_ = false;

  if (sync_core_proxy_) {
    sync_core_proxy_->Disconnect(GetModelType());
    sync_core_proxy_.reset();
  }

  weak_ptr_factory_for_sync_.InvalidateWeakPtrs();
  core_interface_.reset();
}

base::WeakPtr<NonBlockingTypeProcessor>
NonBlockingTypeProcessor::AsWeakPtrForUI() {
  DCHECK(CalledOnValidThread());
  return weak_ptr_factory_for_ui_.GetWeakPtr();
}

void NonBlockingTypeProcessor::OnConnect(
    scoped_ptr<NonBlockingTypeProcessorCoreInterface> core_interface) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);

  is_connected_ = true;
  core_interface_ = core_interface.Pass();

  FlushPendingCommitRequests();
}

void NonBlockingTypeProcessor::Put(const std::string& client_tag,
                                   const sync_pb::EntitySpecifics& specifics) {
  DCHECK_EQ(type_, GetModelTypeFromSpecifics(specifics));

  const std::string client_tag_hash(
      syncable::GenerateSyncableHash(type_, client_tag));

  EntityMap::iterator it = entities_.find(client_tag_hash);
  if (it == entities_.end()) {
    scoped_ptr<ModelThreadSyncEntity> entity(
        ModelThreadSyncEntity::NewLocalItem(
            client_tag, specifics, base::Time::Now()));
    entities_.insert(std::make_pair(client_tag_hash, entity.release()));
  } else {
    ModelThreadSyncEntity* entity = it->second;
    entity->MakeLocalChange(specifics);
  }

  FlushPendingCommitRequests();
}

void NonBlockingTypeProcessor::Delete(const std::string& client_tag) {
  const std::string client_tag_hash(
      syncable::GenerateSyncableHash(type_, client_tag));

  EntityMap::iterator it = entities_.find(client_tag_hash);
  if (it == entities_.end()) {
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as deleted as far as the model is concerned.
    DLOG(WARNING) << "Attempted to delete missing item."
                  << " client tag: " << client_tag;
  } else {
    ModelThreadSyncEntity* entity = it->second;
    entity->Delete();
  }

  FlushPendingCommitRequests();
}

void NonBlockingTypeProcessor::FlushPendingCommitRequests() {
  CommitRequestDataList commit_requests;

  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected())
    return;

  // TODO(rlarocque): Do something smarter than iterate here.
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    if (it->second->RequiresCommitRequest()) {
      CommitRequestData request;
      it->second->InitializeCommitRequestData(&request);
      commit_requests.push_back(request);
      it->second->SetCommitRequestInProgress();
    }
  }

  if (!commit_requests.empty())
    core_interface_->RequestCommits(commit_requests);
}

void NonBlockingTypeProcessor::OnCommitCompletion(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  data_type_state_ = type_state;

  for (CommitResponseDataList::const_iterator list_it = response_list.begin();
       list_it != response_list.end();
       ++list_it) {
    const CommitResponseData& response_data = *list_it;
    const std::string& client_tag_hash = response_data.client_tag_hash;

    EntityMap::iterator it = entities_.find(client_tag_hash);
    if (it == entities_.end()) {
      NOTREACHED() << "Received commit response for missing item."
                   << " type: " << type_ << " client_tag: " << client_tag_hash;
      return;
    } else {
      it->second->ReceiveCommitResponse(response_data.id,
                                        response_data.sequence_number,
                                        response_data.response_version);
    }
  }
}

void NonBlockingTypeProcessor::OnUpdateReceived(
    const DataTypeState& data_type_state,
    const UpdateResponseDataList& response_list) {
  data_type_state_ = data_type_state;

  for (UpdateResponseDataList::const_iterator list_it = response_list.begin();
       list_it != response_list.end();
       ++list_it) {
    const UpdateResponseData& response_data = *list_it;
    const std::string& client_tag_hash = response_data.client_tag_hash;

    EntityMap::iterator it = entities_.find(client_tag_hash);
    if (it == entities_.end()) {
      scoped_ptr<ModelThreadSyncEntity> entity =
          ModelThreadSyncEntity::FromServerUpdate(
              response_data.id,
              response_data.client_tag_hash,
              response_data.non_unique_name,
              response_data.response_version,
              response_data.specifics,
              response_data.deleted,
              response_data.ctime,
              response_data.mtime);
      entities_.insert(std::make_pair(client_tag_hash, entity.release()));
    } else {
      ModelThreadSyncEntity* entity = it->second;
      entity->ApplyUpdateFromServer(response_data.response_version,
                                    response_data.deleted,
                                    response_data.specifics,
                                    response_data.mtime);
      // TODO: Do something special when conflicts are detected.
    }
  }

  // TODO: Inform the model of the new or updated data.
}

}  // namespace syncer
