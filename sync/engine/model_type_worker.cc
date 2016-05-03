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
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "sync/engine/commit_contribution.h"
#include "sync/engine/non_blocking_type_commit_contribution.h"
#include "sync/engine/worker_entity_tracker.h"
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
    std::unique_ptr<Cryptographer> cryptographer,
    NudgeHandler* nudge_handler,
    std::unique_ptr<ModelTypeProcessor> model_type_processor)
    : type_(type),
      data_type_state_(initial_state),
      model_type_processor_(std::move(model_type_processor)),
      cryptographer_(std::move(cryptographer)),
      nudge_handler_(nudge_handler),
      weak_ptr_factory_(this) {
  DCHECK(model_type_processor_);

  // Request an initial sync if it hasn't been completed yet.
  if (!data_type_state_.initial_sync_done()) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  if (cryptographer_) {
    DVLOG(1) << ModelTypeToString(type_) << ": Starting with encryption key "
             << cryptographer_->GetDefaultNigoriKeyName();
    OnCryptographerUpdated();
  }
}

ModelTypeWorker::~ModelTypeWorker() {
  model_type_processor_->DisconnectSync();
}

ModelType ModelTypeWorker::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

bool ModelTypeWorker::IsEncryptionRequired() const {
  return !!cryptographer_;
}

void ModelTypeWorker::UpdateCryptographer(
    std::unique_ptr<Cryptographer> cryptographer) {
  DCHECK(cryptographer);
  cryptographer_ = std::move(cryptographer);

  // Update our state and that of the proxy.
  OnCryptographerUpdated();

  // Nudge the scheduler if we're now allowed to commit.
  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

// UpdateHandler implementation.
bool ModelTypeWorker::IsInitialSyncEnded() const {
  return data_type_state_.initial_sync_done();
}

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

  for (const sync_pb::SyncEntity* update_entity : applicable_updates) {
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

    WorkerEntityTracker* entity = GetOrCreateEntityTracker(data);

    // Check if specifics are encrypted and try to decrypt if so.
    const sync_pb::EntitySpecifics& specifics = update_entity->specifics();
    if (!specifics.has_encrypted()) {
      // No encryption.
      entity->ReceiveUpdate(update_entity->version());
      data.specifics = specifics;
      response_data.entity = data.PassToPtr();
      pending_updates_.push_back(response_data);
    } else if (specifics.has_encrypted() && cryptographer_ &&
               cryptographer_->CanDecrypt(specifics.encrypted())) {
      // Encrypted, but we know the key.
      if (DecryptSpecifics(cryptographer_.get(), specifics, &data.specifics)) {
        entity->ReceiveUpdate(update_entity->version());
        response_data.entity = data.PassToPtr();
        response_data.encryption_key_name = specifics.encrypted().key_name();
        pending_updates_.push_back(response_data);
      }
    } else if (specifics.has_encrypted() &&
               (!cryptographer_ ||
                !cryptographer_->CanDecrypt(specifics.encrypted()))) {
      // Can't decrypt right now.  Ask the entity tracker to handle it.
      data.specifics = specifics;
      response_data.entity = data.PassToPtr();
      entity->ReceiveEncryptedUpdate(response_data);
    }
  }

  return syncer::SYNCER_OK;
}

void ModelTypeWorker::ApplyUpdates(syncer::sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // This should only ever be called after one PassiveApplyUpdates.
  DCHECK(data_type_state_.initial_sync_done());
  // Download cycle is done, pass all updates to the processor.
  ApplyPendingUpdates();
}

void ModelTypeWorker::PassiveApplyUpdates(
    syncer::sessions::StatusController* status) {
  // This should only be called at the end of the very first download cycle.
  DCHECK(!data_type_state_.initial_sync_done());
  // Indicate to the processor that the initial download is done. The initial
  // sync technically isn't done yet but by the time this value is persisted to
  // disk on the model thread it will be.
  data_type_state_.set_initial_sync_done(true);
  ApplyPendingUpdates();
}

void ModelTypeWorker::ApplyPendingUpdates() {
  DVLOG(1) << ModelTypeToString(type_) << ": "
           << base::StringPrintf("Delivering %" PRIuS " applicable updates.",
                                 pending_updates_.size());
  model_type_processor_->OnUpdateReceived(data_type_state_, pending_updates_);
  pending_updates_.clear();
}

void ModelTypeWorker::EnqueueForCommit(const CommitRequestDataList& list) {
  DCHECK(CalledOnValidThread());

  DCHECK(IsTypeInitialized())
      << "Asked to commit items before type was initialized.  "
      << "ModelType is: " << ModelTypeToString(type_);

  for (const CommitRequestData& commit : list) {
    const EntityData& data = commit.entity.value();
    if (!data.is_deleted()) {
      DCHECK_EQ(type_, syncer::GetModelTypeFromSpecifics(data.specifics));
    }
    GetOrCreateEntityTracker(data)->RequestCommit(commit);
  }

  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

// CommitContributor implementation.
std::unique_ptr<CommitContribution> ModelTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK(CalledOnValidThread());
  // There shouldn't be a GetUpdates in progress when a commit is triggered.
  DCHECK(pending_updates_.empty());

  size_t space_remaining = max_entries;
  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> commit_entities;

  if (!CanCommitItems())
    return std::unique_ptr<CommitContribution>();

  // TODO(rlarocque): Avoid iterating here.
  for (EntityMap::const_iterator it = entities_.begin();
       it != entities_.end() && space_remaining > 0; ++it) {
    WorkerEntityTracker* entity = it->second.get();
    if (entity->HasPendingCommit()) {
      sync_pb::SyncEntity* commit_entity = commit_entities.Add();
      entity->PopulateCommitProto(commit_entity);
      AdjustCommitProto(commit_entity);
      space_remaining--;
    }
  }

  if (commit_entities.size() == 0)
    return std::unique_ptr<CommitContribution>();

  return std::unique_ptr<CommitContribution>(
      new NonBlockingTypeCommitContribution(data_type_state_.type_context(),
                                            commit_entities, this));
}

void ModelTypeWorker::OnCommitResponse(CommitResponseDataList* response_list) {
  for (CommitResponseData& response : *response_list) {
    WorkerEntityTracker* entity = GetEntityTracker(response.client_tag_hash);

    // There's no way we could have committed an entry we know nothing about.
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for item unknown to us."
                   << " Model type: " << ModelTypeToString(type_)
                   << " ID: " << response.id;
      continue;
    }

    entity->ReceiveCommitResponse(&response);
  }

  // Send the responses back to the model thread.  It needs to know which
  // items have been successfully committed so it can save that information in
  // permanent storage.
  model_type_processor_->OnCommitCompleted(data_type_state_, *response_list);
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

void ModelTypeWorker::AdjustCommitProto(sync_pb::SyncEntity* sync_entity) {
  DCHECK(CanCommitItems());

  // Initial commits need our help to generate a client ID.
  if (sync_entity->version() == kUncommittedVersion) {
    DCHECK(!sync_entity->has_id_string());
    // TODO(stanisc): This is incorrect for bookmarks for two reasons:
    // 1) Won't be able to match previously committed bookmarks to the ones
    //    with server ID.
    // 2) Recommitting an item in a case of failing to receive commit response
    //    would result in generating a different client ID, which in turn
    //    would result in a duplication.
    // We should generate client ID on the frontend side instead.
    sync_entity->set_id_string(base::GenerateGUID());
    sync_entity->set_version(0);
  } else {
    DCHECK(sync_entity->has_id_string());
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
    if (it->second->HasEncryptedUpdate()) {
      const UpdateResponseData& encrypted_update =
          it->second->GetEncryptedUpdate();
      const EntityData& data = encrypted_update.entity.value();

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

          UpdateResponseData decrypted_update;
          decrypted_update.entity = decrypted_data.PassToPtr();
          decrypted_update.response_version = encrypted_update.response_version;
          decrypted_update.encryption_key_name =
              data.specifics.encrypted().key_name();
          response_datas.push_back(decrypted_update);

          it->second->ClearEncryptedUpdate();
        }
      }
    }
  }

  if (new_encryption_key || response_datas.size() > 0) {
    DVLOG(1) << ModelTypeToString(type_) << ": "
             << base::StringPrintf("Delivering encryption key and %" PRIuS
                                   " decrypted updates.",
                                   response_datas.size());
    model_type_processor_->OnUpdateReceived(data_type_state_, response_datas);
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

WorkerEntityTracker* ModelTypeWorker::GetEntityTracker(
    const std::string& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

WorkerEntityTracker* ModelTypeWorker::CreateEntityTracker(
    const EntityData& data) {
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  std::unique_ptr<WorkerEntityTracker> entity =
      base::WrapUnique(new WorkerEntityTracker(data.id, data.client_tag_hash));
  WorkerEntityTracker* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  return entity_ptr;
}

WorkerEntityTracker* ModelTypeWorker::GetOrCreateEntityTracker(
    const EntityData& data) {
  WorkerEntityTracker* entity = GetEntityTracker(data.client_tag_hash);
  return entity ? entity : CreateEntityTracker(data);
}

}  // namespace syncer_v2
