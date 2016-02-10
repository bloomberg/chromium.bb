// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_worker.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sync/engine/commit_contribution.h"
#include "sync/engine/entity_tracker.h"
#include "sync/engine/non_blocking_type_commit_contribution.h"
#include "sync/internal_api/public/model_type_processor.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

namespace syncer_v2 {

using syncer::CommitContribution;
using syncer::Cryptographer;
using syncer::ModelType;
using syncer::NudgeHandler;
using syncer::SyncerError;

ModelTypeWorker::ModelTypeWorker(
    ModelType type,
    const sync_pb::DataTypeState& initial_state,
    const UpdateResponseDataList& saved_pending_updates,
    scoped_ptr<Cryptographer> cryptographer,
    NudgeHandler* nudge_handler,
    scoped_ptr<ModelTypeProcessor> model_type_processor)
    : type_(type),
      data_type_state_(initial_state),
      model_type_processor_(std::move(model_type_processor)),
      cryptographer_(std::move(cryptographer)),
      nudge_handler_(nudge_handler),
      weak_ptr_factory_(this) {
  // Request an initial sync if it hasn't been completed yet.
  if (!data_type_state_.initial_sync_done()) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  for (UpdateResponseDataList::const_iterator it =
           saved_pending_updates.begin();
       it != saved_pending_updates.end(); ++it) {
    scoped_ptr<EntityTracker> entity_tracker =
        EntityTracker::FromUpdateResponse(*it);
    entity_tracker->ReceivePendingUpdate(*it);
    entities_.insert(
        std::make_pair(it->entity->client_tag_hash, std::move(entity_tracker)));
  }

  if (cryptographer_) {
    DVLOG(1) << ModelTypeToString(type_) << ": Starting with encryption key "
             << cryptographer_->GetDefaultNigoriKeyName();
    OnCryptographerUpdated();
  }
}

ModelTypeWorker::~ModelTypeWorker() {}

ModelType ModelTypeWorker::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

bool ModelTypeWorker::IsEncryptionRequired() const {
  return !!cryptographer_;
}

void ModelTypeWorker::UpdateCryptographer(
    scoped_ptr<Cryptographer> cryptographer) {
  DCHECK(cryptographer);
  cryptographer_ = std::move(cryptographer);

  // Update our state and that of the proxy.
  OnCryptographerUpdated();

  // Nudge the scheduler if we're now allowed to commit.
  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

// UpdateHandler implementation.
void ModelTypeWorker::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  DCHECK(CalledOnValidThread());
  progress_marker->CopyFrom(data_type_state_.progress_marker());
}

void ModelTypeWorker::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  DCHECK(CalledOnValidThread());
  context->CopyFrom(data_type_state_.type_context());
}

SyncerError ModelTypeWorker::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    syncer::sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());

  // TODO(rlarocque): Handle data type context conflicts.
  *data_type_state_.mutable_type_context() = mutated_context;
  *data_type_state_.mutable_progress_marker() = progress_marker;

  UpdateResponseDataList response_datas;
  UpdateResponseDataList pending_updates;

  for (SyncEntityList::const_iterator update_it = applicable_updates.begin();
       update_it != applicable_updates.end(); ++update_it) {
    const sync_pb::SyncEntity* update_entity = *update_it;

    // Skip updates for permanent folders.
    // TODO(stanisc): crbug.com/516866: might need to handle this for
    // hierarchical datatypes.
    if (!update_entity->server_defined_unique_tag().empty())
      continue;

    // Normal updates are handled here.
    const std::string& client_tag_hash =
        update_entity->client_defined_unique_tag();

    // TODO(stanisc): crbug.com/516866: this wouldn't be true for bookmarks.
    DCHECK(!client_tag_hash.empty());

    // Prepare the message for the model thread.
    EntityData data;
    data.id = update_entity->id_string();
    data.client_tag_hash = client_tag_hash;
    data.creation_time = syncer::ProtoTimeToTime(update_entity->ctime());
    data.modification_time = syncer::ProtoTimeToTime(update_entity->mtime());
    data.non_unique_name = update_entity->name();

    UpdateResponseData response_data;
    response_data.response_version = update_entity->version();

    EntityTracker* entity_tracker = nullptr;
    EntityMap::const_iterator map_it = entities_.find(client_tag_hash);
    if (map_it == entities_.end()) {
      scoped_ptr<EntityTracker> scoped_entity_tracker =
          EntityTracker::FromUpdateResponse(response_data);
      entity_tracker = scoped_entity_tracker.get();
      entities_.insert(
          std::make_pair(client_tag_hash, std::move(scoped_entity_tracker)));
    } else {
      entity_tracker = map_it->second.get();
    }

    const sync_pb::EntitySpecifics& specifics = update_entity->specifics();

    if (!specifics.has_encrypted()) {
      // No encryption.
      entity_tracker->ReceiveUpdate(update_entity->version());
      data.specifics = specifics;
      response_data.entity = data.Pass();
      response_datas.push_back(response_data);
    } else if (specifics.has_encrypted() && cryptographer_ &&
               cryptographer_->CanDecrypt(specifics.encrypted())) {
      // Encrypted, but we know the key.
      if (DecryptSpecifics(cryptographer_.get(), specifics, &data.specifics)) {
        entity_tracker->ReceiveUpdate(update_entity->version());
        response_data.entity = data.Pass();
        response_data.encryption_key_name = specifics.encrypted().key_name();
        response_datas.push_back(response_data);
      }
    } else if (specifics.has_encrypted() &&
               (!cryptographer_ ||
                !cryptographer_->CanDecrypt(specifics.encrypted()))) {
      // Can't decrypt right now.  Ask the entity tracker to handle it.
      data.specifics = specifics;
      response_data.entity = data.Pass();
      if (entity_tracker->ReceivePendingUpdate(response_data)) {
        // Send to the model thread for safe-keeping across restarts if the
        // tracker decides the update is worth keeping.
        pending_updates.push_back(response_data);
      }
    }
  }

  DVLOG(1) << ModelTypeToString(type_) << ": "
           << base::StringPrintf("Delivering %" PRIuS " applicable and %" PRIuS
                                 " pending updates.",
                                 response_datas.size(), pending_updates.size());

  // Forward these updates to the model thread so it can do the rest.
  model_type_processor_->OnUpdateReceived(data_type_state_, response_datas,
                                          pending_updates);

  return syncer::SYNCER_OK;
}

void ModelTypeWorker::ApplyUpdates(syncer::sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // This function is called only when we've finished a download cycle, ie. we
  // got a response with changes_remaining == 0.  If this is our first download
  // cycle, we should update our state so the ModelTypeProcessor knows that
  // it's safe to commit items now.
  if (!data_type_state_.initial_sync_done()) {
    DVLOG(1) << "Delivering 'initial sync done' ping.";

    data_type_state_.set_initial_sync_done(true);

    model_type_processor_->OnUpdateReceived(
        data_type_state_, UpdateResponseDataList(), UpdateResponseDataList());
  }
}

void ModelTypeWorker::PassiveApplyUpdates(
    syncer::sessions::StatusController* status) {
  NOTREACHED()
      << "Non-blocking types should never apply updates on sync thread.  "
      << "ModelType is: " << ModelTypeToString(type_);
}

void ModelTypeWorker::EnqueueForCommit(const CommitRequestDataList& list) {
  DCHECK(CalledOnValidThread());

  DCHECK(IsTypeInitialized())
      << "Asked to commit items before type was initialized.  "
      << "ModelType is: " << ModelTypeToString(type_);

  for (CommitRequestDataList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    StorePendingCommit(*it);
  }

  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

// CommitContributor implementation.
scoped_ptr<CommitContribution> ModelTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK(CalledOnValidThread());

  size_t space_remaining = max_entries;
  std::vector<int64_t> sequence_numbers;
  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> commit_entities;

  if (!CanCommitItems())
    return scoped_ptr<CommitContribution>();

  // TODO(rlarocque): Avoid iterating here.
  for (EntityMap::const_iterator it = entities_.begin();
       it != entities_.end() && space_remaining > 0; ++it) {
    EntityTracker* entity = it->second.get();
    if (entity->HasPendingCommit()) {
      sync_pb::SyncEntity* commit_entity = commit_entities.Add();
      int64_t sequence_number = -1;

      entity->PrepareCommitProto(commit_entity, &sequence_number);
      HelpInitializeCommitEntity(commit_entity);
      sequence_numbers.push_back(sequence_number);

      space_remaining--;
    }
  }

  if (commit_entities.size() == 0)
    return scoped_ptr<CommitContribution>();

  return scoped_ptr<CommitContribution>(new NonBlockingTypeCommitContribution(
      data_type_state_.type_context(), commit_entities, sequence_numbers,
      this));
}

void ModelTypeWorker::StorePendingCommit(const CommitRequestData& request) {
  const EntityData& data = request.entity.value();
  if (!data.is_deleted()) {
    DCHECK_EQ(type_, syncer::GetModelTypeFromSpecifics(data.specifics));
  }

  EntityTracker* entity;
  EntityMap::const_iterator map_it = entities_.find(data.client_tag_hash);
  if (map_it == entities_.end()) {
    scoped_ptr<EntityTracker> scoped_entity =
        EntityTracker::FromCommitRequest(request);
    entity = scoped_entity.get();
    entities_.insert(
        std::make_pair(data.client_tag_hash, std::move(scoped_entity)));
  } else {
    entity = map_it->second.get();
  }
  entity->RequestCommit(request);
}

void ModelTypeWorker::OnCommitResponse(
    const CommitResponseDataList& response_list) {
  for (CommitResponseDataList::const_iterator response_it =
           response_list.begin();
       response_it != response_list.end(); ++response_it) {
    const std::string client_tag_hash = response_it->client_tag_hash;
    EntityMap::const_iterator map_it = entities_.find(client_tag_hash);

    // There's no way we could have committed an entry we know nothing about.
    if (map_it == entities_.end()) {
      NOTREACHED() << "Received commit response for item unknown to us."
                   << " Model type: " << ModelTypeToString(type_)
                   << " ID: " << response_it->id;
      continue;
    }

    EntityTracker* entity = map_it->second.get();
    entity->ReceiveCommitResponse(response_it->id,
                                  response_it->response_version,
                                  response_it->sequence_number);
  }

  // Send the responses back to the model thread.  It needs to know which
  // items have been successfully committed so it can save that information in
  // permanent storage.
  model_type_processor_->OnCommitCompleted(data_type_state_, response_list);
}

base::WeakPtr<ModelTypeWorker> ModelTypeWorker::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ModelTypeWorker::IsTypeInitialized() const {
  return data_type_state_.initial_sync_done() &&
         !data_type_state_.progress_marker().token().empty();
}

bool ModelTypeWorker::CanCommitItems() const {
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

void ModelTypeWorker::HelpInitializeCommitEntity(
    sync_pb::SyncEntity* sync_entity) {
  DCHECK(CanCommitItems());

  // Initial commits need our help to generate a client ID.
  if (!sync_entity->has_id_string()) {
    DCHECK_EQ(kUncommittedVersion, sync_entity->version());
    // TODO(stanisc): This is incorrect for bookmarks for two reasons:
    // 1) Won't be able to match previously committed bookmarks to the ones
    //    with server ID.
    // 2) Recommitting an item in a case of failing to receive commit response
    //    would result in generating a different client ID, which in turn
    //    would result in a duplication.
    // We should generate client ID on the frontend side instead.
    sync_entity->set_id_string(base::GenerateGUID());
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

  // TODO(stanisc): crbug.com/516866:
  // Call sync_entity->set_parent_id_string(...) for hierarchical entities here.
}

void ModelTypeWorker::OnCryptographerUpdated() {
  DCHECK(cryptographer_);

  bool new_encryption_key = false;
  UpdateResponseDataList response_datas;

  const std::string& new_key_name = cryptographer_->GetDefaultNigoriKeyName();

  // Handle a change in encryption key.
  if (data_type_state_.encryption_key_name() != new_key_name) {
    DVLOG(1) << ModelTypeToString(type_) << ": Updating encryption key "
             << data_type_state_.encryption_key_name() << " -> "
             << new_key_name;
    data_type_state_.set_encryption_key_name(new_key_name);
    new_encryption_key = true;
  }

  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    if (it->second->HasPendingUpdate()) {
      const UpdateResponseData& saved_pending = it->second->GetPendingUpdate();
      const EntityData& data = saved_pending.entity.value();

      // We assume all pending updates are encrypted items for which we
      // don't have the key.
      DCHECK(data.specifics.has_encrypted());

      if (cryptographer_->CanDecrypt(data.specifics.encrypted())) {
        EntityData decrypted_data;
        if (DecryptSpecifics(cryptographer_.get(), data.specifics,
                             &decrypted_data.specifics)) {
          // Copy other fields one by one since EntityData doesn't allow
          // copying.
          // TODO(stanisc): this code is likely to be removed once we get
          // rid of pending updates.
          decrypted_data.id = data.id;
          decrypted_data.client_tag_hash = data.client_tag_hash;
          decrypted_data.non_unique_name = data.non_unique_name;
          decrypted_data.creation_time = data.creation_time;
          decrypted_data.modification_time = data.modification_time;

          UpdateResponseData decrypted_response;
          decrypted_response.entity = decrypted_data.Pass();
          decrypted_response.response_version = saved_pending.response_version;
          decrypted_response.encryption_key_name =
              data.specifics.encrypted().key_name();
          response_datas.push_back(decrypted_response);

          it->second->ClearPendingUpdate();
        }
      }
    }
  }

  if (new_encryption_key || response_datas.size() > 0) {
    DVLOG(1) << ModelTypeToString(type_) << ": "
             << base::StringPrintf("Delivering encryption key and %" PRIuS
                                   " decrypted updates.",
                                   response_datas.size());
    model_type_processor_->OnUpdateReceived(data_type_state_, response_datas,
                                            UpdateResponseDataList());
  }
}

bool ModelTypeWorker::DecryptSpecifics(Cryptographer* cryptographer,
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

}  // namespace syncer_v2
