// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_sync_proxy_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "sync/engine/model_type_entity.h"
#include "sync/engine/model_type_sync_worker.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/syncable/syncable_util.h"

namespace syncer {

ModelTypeSyncProxyImpl::ModelTypeSyncProxyImpl(ModelType type)
    : type_(type),
      is_preferred_(false),
      is_connected_(false),
      entities_deleter_(&entities_),
      pending_updates_map_deleter_(&pending_updates_map_),
      weak_ptr_factory_for_ui_(this),
      weak_ptr_factory_for_sync_(this) {
}

ModelTypeSyncProxyImpl::~ModelTypeSyncProxyImpl() {
}

bool ModelTypeSyncProxyImpl::IsPreferred() const {
  DCHECK(CalledOnValidThread());
  return is_preferred_;
}

bool ModelTypeSyncProxyImpl::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return is_connected_;
}

ModelType ModelTypeSyncProxyImpl::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

void ModelTypeSyncProxyImpl::Enable(
    scoped_ptr<SyncContextProxy> sync_context_proxy) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to enable " << ModelTypeToString(type_);

  is_preferred_ = true;

  // TODO(rlarocque): At some point, this should be loaded from storage.
  data_type_state_.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(type_));

  UpdateResponseDataList saved_pending_updates = GetPendingUpdates();
  sync_context_proxy_ = sync_context_proxy.Pass();
  sync_context_proxy_->ConnectTypeToSync(
      GetModelType(),
      data_type_state_,
      saved_pending_updates,
      weak_ptr_factory_for_sync_.GetWeakPtr());
}

void ModelTypeSyncProxyImpl::Disable() {
  DCHECK(CalledOnValidThread());
  is_preferred_ = false;
  Disconnect();

  ClearSyncState();
}

void ModelTypeSyncProxyImpl::Disconnect() {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to disconnect " << ModelTypeToString(type_);
  is_connected_ = false;

  if (sync_context_proxy_) {
    sync_context_proxy_->Disconnect(GetModelType());
    sync_context_proxy_.reset();
  }

  weak_ptr_factory_for_sync_.InvalidateWeakPtrs();
  worker_.reset();

  ClearTransientSyncState();
}

base::WeakPtr<ModelTypeSyncProxyImpl> ModelTypeSyncProxyImpl::AsWeakPtrForUI() {
  DCHECK(CalledOnValidThread());
  return weak_ptr_factory_for_ui_.GetWeakPtr();
}

void ModelTypeSyncProxyImpl::OnConnect(scoped_ptr<ModelTypeSyncWorker> worker) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);

  is_connected_ = true;
  worker_ = worker.Pass();

  FlushPendingCommitRequests();
}

void ModelTypeSyncProxyImpl::Put(const std::string& client_tag,
                                 const sync_pb::EntitySpecifics& specifics) {
  DCHECK_EQ(type_, GetModelTypeFromSpecifics(specifics));

  const std::string client_tag_hash(
      syncable::GenerateSyncableHash(type_, client_tag));

  EntityMap::iterator it = entities_.find(client_tag_hash);
  if (it == entities_.end()) {
    scoped_ptr<ModelTypeEntity> entity(ModelTypeEntity::NewLocalItem(
        client_tag, specifics, base::Time::Now()));
    entities_.insert(std::make_pair(client_tag_hash, entity.release()));
  } else {
    ModelTypeEntity* entity = it->second;
    entity->MakeLocalChange(specifics);
  }

  FlushPendingCommitRequests();
}

void ModelTypeSyncProxyImpl::Delete(const std::string& client_tag) {
  const std::string client_tag_hash(
      syncable::GenerateSyncableHash(type_, client_tag));

  EntityMap::iterator it = entities_.find(client_tag_hash);
  if (it == entities_.end()) {
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as deleted as far as the model is concerned.
    DLOG(WARNING) << "Attempted to delete missing item."
                  << " client tag: " << client_tag;
  } else {
    ModelTypeEntity* entity = it->second;
    entity->Delete();
  }

  FlushPendingCommitRequests();
}

void ModelTypeSyncProxyImpl::FlushPendingCommitRequests() {
  CommitRequestDataList commit_requests;

  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected())
    return;

  // Don't send anything if the type is not ready to handle commits.
  if (!data_type_state_.initial_sync_done)
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
    worker_->EnqueueForCommit(commit_requests);
}

void ModelTypeSyncProxyImpl::OnCommitCompleted(
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
                                        response_data.response_version,
                                        data_type_state_.encryption_key_name);
    }
  }
}

void ModelTypeSyncProxyImpl::OnUpdateReceived(
    const DataTypeState& data_type_state,
    const UpdateResponseDataList& response_list,
    const UpdateResponseDataList& pending_updates) {
  bool got_new_encryption_requirements = data_type_state_.encryption_key_name !=
                                         data_type_state.encryption_key_name;

  data_type_state_ = data_type_state;

  for (UpdateResponseDataList::const_iterator list_it = response_list.begin();
       list_it != response_list.end();
       ++list_it) {
    const UpdateResponseData& response_data = *list_it;
    const std::string& client_tag_hash = response_data.client_tag_hash;

    UpdateMap::iterator old_it = pending_updates_map_.find(client_tag_hash);
    if (old_it != pending_updates_map_.end()) {
      // If we're being asked to apply an update to this entity, this overrides
      // the previous pending updates.
      delete old_it->second;
      pending_updates_map_.erase(old_it);
    }

    EntityMap::iterator it = entities_.find(client_tag_hash);
    if (it == entities_.end()) {
      scoped_ptr<ModelTypeEntity> entity =
          ModelTypeEntity::FromServerUpdate(response_data.id,
                                            response_data.client_tag_hash,
                                            response_data.non_unique_name,
                                            response_data.response_version,
                                            response_data.specifics,
                                            response_data.deleted,
                                            response_data.ctime,
                                            response_data.mtime,
                                            response_data.encryption_key_name);
      entities_.insert(std::make_pair(client_tag_hash, entity.release()));
    } else {
      ModelTypeEntity* entity = it->second;
      entity->ApplyUpdateFromServer(response_data.response_version,
                                    response_data.deleted,
                                    response_data.specifics,
                                    response_data.mtime,
                                    response_data.encryption_key_name);

      // TODO: Do something special when conflicts are detected.
    }

    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (data_type_state_.encryption_key_name !=
        response_data.encryption_key_name) {
      DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
               << response_data.encryption_key_name << " -> "
               << data_type_state_.encryption_key_name;
      EntityMap::iterator it2 = entities_.find(client_tag_hash);
      it2->second->UpdateDesiredEncryptionKey(
          data_type_state_.encryption_key_name);
    }
  }

  // Save pending updates in the appropriate data structure.
  for (UpdateResponseDataList::const_iterator list_it = pending_updates.begin();
       list_it != pending_updates.end();
       ++list_it) {
    const UpdateResponseData& update = *list_it;
    const std::string& client_tag_hash = update.client_tag_hash;

    UpdateMap::iterator lookup_it = pending_updates_map_.find(client_tag_hash);
    if (lookup_it == pending_updates_map_.end()) {
      pending_updates_map_.insert(
          std::make_pair(client_tag_hash, new UpdateResponseData(update)));
    } else if (lookup_it->second->response_version <= update.response_version) {
      delete lookup_it->second;
      pending_updates_map_.erase(lookup_it);
      pending_updates_map_.insert(
          std::make_pair(client_tag_hash, new UpdateResponseData(update)));
    } else {
      // Received update is stale, do not overwrite existing.
    }
  }

  if (got_new_encryption_requirements) {
    for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
         ++it) {
      it->second->UpdateDesiredEncryptionKey(
          data_type_state_.encryption_key_name);
    }
  }

  // We may have new reasons to commit by the time this function is done.
  FlushPendingCommitRequests();

  // TODO: Inform the model of the new or updated data.
  // TODO: Persist the new data on disk.
}

UpdateResponseDataList ModelTypeSyncProxyImpl::GetPendingUpdates() {
  UpdateResponseDataList pending_updates_list;
  for (UpdateMap::const_iterator it = pending_updates_map_.begin();
       it != pending_updates_map_.end();
       ++it) {
    pending_updates_list.push_back(*it->second);
  }
  return pending_updates_list;
}

void ModelTypeSyncProxyImpl::ClearTransientSyncState() {
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    it->second->ClearTransientSyncState();
  }
}

void ModelTypeSyncProxyImpl::ClearSyncState() {
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    it->second->ClearSyncState();
  }
  STLDeleteValues(&pending_updates_map_);
  data_type_state_ = DataTypeState();
}

}  // namespace syncer
