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
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

namespace syncer {

ModelTypeSyncWorkerImpl::ModelTypeSyncWorkerImpl(
    ModelType type,
    const DataTypeState& initial_state,
    const UpdateResponseDataList& saved_pending_updates,
    scoped_ptr<Cryptographer> cryptographer,
    NudgeHandler* nudge_handler,
    scoped_ptr<ModelTypeSyncProxy> type_sync_proxy)
    : type_(type),
      data_type_state_(initial_state),
      type_sync_proxy_(type_sync_proxy.Pass()),
      cryptographer_(cryptographer.Pass()),
      nudge_handler_(nudge_handler),
      entities_deleter_(&entities_),
      weak_ptr_factory_(this) {
  // Request an initial sync if it hasn't been completed yet.
  if (!data_type_state_.initial_sync_done) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  for (UpdateResponseDataList::const_iterator it =
           saved_pending_updates.begin();
       it != saved_pending_updates.end();
       ++it) {
    EntityTracker* entity_tracker = EntityTracker::FromServerUpdate(
        it->id, it->client_tag_hash, it->response_version);
    entity_tracker->ReceivePendingUpdate(*it);
    entities_.insert(std::make_pair(it->client_tag_hash, entity_tracker));
  }

  if (cryptographer_) {
    DVLOG(1) << ModelTypeToString(type_) << ": Starting with encryption key "
             << cryptographer_->GetDefaultNigoriKeyName();
    OnCryptographerUpdated();
  }
}

ModelTypeSyncWorkerImpl::~ModelTypeSyncWorkerImpl() {
}

ModelType ModelTypeSyncWorkerImpl::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

bool ModelTypeSyncWorkerImpl::IsEncryptionRequired() const {
  return !!cryptographer_;
}

void ModelTypeSyncWorkerImpl::UpdateCryptographer(
    scoped_ptr<Cryptographer> cryptographer) {
  DCHECK(cryptographer);
  cryptographer_ = cryptographer.Pass();

  // Update our state and that of the proxy.
  OnCryptographerUpdated();

  // Nudge the scheduler if we're now allowed to commit.
  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
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
  UpdateResponseDataList pending_updates;

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

      EntityTracker* entity_tracker = NULL;
      EntityMap::const_iterator map_it = entities_.find(client_tag_hash);
      if (map_it == entities_.end()) {
        entity_tracker =
            EntityTracker::FromServerUpdate(update_entity->id_string(),
                                            client_tag_hash,
                                            update_entity->version());
        entities_.insert(std::make_pair(client_tag_hash, entity_tracker));
      } else {
        entity_tracker = map_it->second;
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

      const sync_pb::EntitySpecifics& specifics = update_entity->specifics();

      if (!specifics.has_encrypted()) {
        // No encryption.
        entity_tracker->ReceiveUpdate(update_entity->version());
        response_data.specifics = specifics;
        response_datas.push_back(response_data);
      } else if (specifics.has_encrypted() && cryptographer_ &&
                 cryptographer_->CanDecrypt(specifics.encrypted())) {
        // Encrypted, but we know the key.
        if (DecryptSpecifics(
                cryptographer_.get(), specifics, &response_data.specifics)) {
          entity_tracker->ReceiveUpdate(update_entity->version());
          response_data.encryption_key_name = specifics.encrypted().key_name();
          response_datas.push_back(response_data);
        }
      } else if (specifics.has_encrypted() &&
                 (!cryptographer_ ||
                  !cryptographer_->CanDecrypt(specifics.encrypted()))) {
        // Can't decrypt right now.  Ask the entity tracker to handle it.
        response_data.specifics = specifics;
        if (entity_tracker->ReceivePendingUpdate(response_data)) {
          // Send to the model thread for safe-keeping across restarts if the
          // tracker decides the update is worth keeping.
          pending_updates.push_back(response_data);
        }
      }
    }
  }

  DVLOG(1) << ModelTypeToString(type_) << ": "
           << base::StringPrintf(
                  "Delivering %zd applicable and %zd pending updates.",
                  response_datas.size(),
                  pending_updates.size());

  // Forward these updates to the model thread so it can do the rest.
  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, response_datas, pending_updates);

  return SYNCER_OK;
}

void ModelTypeSyncWorkerImpl::ApplyUpdates(sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // This function is called only when we've finished a download cycle, ie. we
  // got a response with changes_remaining == 0.  If this is our first download
  // cycle, we should update our state so the ModelTypeSyncProxy knows that
  // it's safe to commit items now.
  if (!data_type_state_.initial_sync_done) {
    DVLOG(1) << "Delivering 'initial sync done' ping.";

    data_type_state_.initial_sync_done = true;

    type_sync_proxy_->OnUpdateReceived(
        data_type_state_, UpdateResponseDataList(), UpdateResponseDataList());
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

  DCHECK(IsTypeInitialized())
      << "Asked to commit items before type was initialized.  "
      << "ModelType is: " << ModelTypeToString(type_);

  for (CommitRequestDataList::const_iterator it = list.begin();
       it != list.end();
       ++it) {
    StorePendingCommit(*it);
  }

  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
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

bool ModelTypeSyncWorkerImpl::IsTypeInitialized() const {
  return !data_type_state_.type_root_id.empty() &&
         data_type_state_.initial_sync_done;
}

bool ModelTypeSyncWorkerImpl::CanCommitItems() const {
  // We can't commit anything until we know the type's parent node.
  // We'll get it in the first update response.
  if (!IsTypeInitialized())
    return false;

  // Don't commit if we should be encrypting but don't have the required keys.
  if (IsEncryptionRequired() &&
      (!cryptographer_ || !cryptographer_->is_ready())) {
    return false;
  }

  return true;
}

void ModelTypeSyncWorkerImpl::HelpInitializeCommitEntity(
    sync_pb::SyncEntity* sync_entity) {
  DCHECK(CanCommitItems());

  // Initial commits need our help to generate a client ID.
  if (!sync_entity->has_id_string()) {
    DCHECK_EQ(kUncommittedVersion, sync_entity->version());
    const int64 id = data_type_state_.next_client_id++;
    sync_entity->set_id_string(
        base::StringPrintf("%s-%" PRId64, ModelTypeToString(type_), id));
  }

  // Encrypt the specifics and hide the title if necessary.
  if (IsEncryptionRequired()) {
    // IsEncryptionRequired() && CanCommitItems() implies
    // that the cryptographer is valid and ready to encrypt.
    sync_pb::EntitySpecifics encrypted_specifics;
    bool result = cryptographer_->Encrypt(
        sync_entity->specifics(), encrypted_specifics.mutable_encrypted());
    DCHECK(result);
    sync_entity->mutable_specifics()->CopyFrom(encrypted_specifics);
    sync_entity->set_name("encrypted");
  }

  // Always include enough specifics to identify the type.  Do this even in
  // deletion requests, where the specifics are otherwise invalid.
  AddDefaultFieldValue(type_, sync_entity->mutable_specifics());

  // We're always responsible for the parent ID.
  sync_entity->set_parent_id_string(data_type_state_.type_root_id);
}

void ModelTypeSyncWorkerImpl::OnCryptographerUpdated() {
  DCHECK(cryptographer_);

  bool new_encryption_key = false;
  UpdateResponseDataList response_datas;

  const std::string& new_key_name = cryptographer_->GetDefaultNigoriKeyName();

  // Handle a change in encryption key.
  if (data_type_state_.encryption_key_name != new_key_name) {
    DVLOG(1) << ModelTypeToString(type_) << ": Updating encryption key "
             << data_type_state_.encryption_key_name << " -> " << new_key_name;
    data_type_state_.encryption_key_name = new_key_name;
    new_encryption_key = true;
  }

  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    if (it->second->HasPendingUpdate()) {
      const UpdateResponseData& saved_pending = it->second->GetPendingUpdate();

      // We assume all pending updates are encrypted items for which we
      // don't have the key.
      DCHECK(saved_pending.specifics.has_encrypted());

      if (cryptographer_->CanDecrypt(saved_pending.specifics.encrypted())) {
        UpdateResponseData decrypted_response = saved_pending;
        if (DecryptSpecifics(cryptographer_.get(),
                             saved_pending.specifics,
                             &decrypted_response.specifics)) {
          decrypted_response.encryption_key_name =
              saved_pending.specifics.encrypted().key_name();
          response_datas.push_back(decrypted_response);

          it->second->ClearPendingUpdate();
        }
      }
    }
  }

  if (new_encryption_key || response_datas.size() > 0) {
    DVLOG(1) << ModelTypeToString(type_) << ": "
             << base::StringPrintf(
                    "Delivering encryption key and %zd decrypted updates.",
                    response_datas.size());
    type_sync_proxy_->OnUpdateReceived(
        data_type_state_, response_datas, UpdateResponseDataList());
  }
}

bool ModelTypeSyncWorkerImpl::DecryptSpecifics(
    Cryptographer* cryptographer,
    const sync_pb::EntitySpecifics& in,
    sync_pb::EntitySpecifics* out) {
  DCHECK(in.has_encrypted());
  DCHECK(cryptographer->CanDecrypt(in.encrypted()));

  std::string plaintext;
  plaintext = cryptographer->DecryptToString(in.encrypted());
  if (plaintext.empty()) {
    LOG(ERROR) << "Failed to decrypt a decryptable entity";
    return false;
  }
  if (!out->ParseFromString(plaintext)) {
    LOG(ERROR) << "Failed to parse decrypted entity";
    return false;
  }
  return true;
}

}  // namespace syncer
