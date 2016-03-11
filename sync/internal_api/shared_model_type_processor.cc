// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/shared_model_type_processor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/processor_entity_tracker.h"
#include "sync/syncable/syncable_util.h"

namespace syncer_v2 {

namespace {

class ModelTypeProcessorProxy : public ModelTypeProcessor {
 public:
  ModelTypeProcessorProxy(
      const base::WeakPtr<ModelTypeProcessor>& processor,
      const scoped_refptr<base::SequencedTaskRunner>& processor_task_runner);
  ~ModelTypeProcessorProxy() override;

  void ConnectSync(scoped_ptr<CommitQueue> worker) override;
  void OnCommitCompleted(const sync_pb::DataTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::DataTypeState& type_state,
                        const UpdateResponseDataList& updates) override;

 private:
  base::WeakPtr<ModelTypeProcessor> processor_;
  scoped_refptr<base::SequencedTaskRunner> processor_task_runner_;
};

ModelTypeProcessorProxy::ModelTypeProcessorProxy(
    const base::WeakPtr<ModelTypeProcessor>& processor,
    const scoped_refptr<base::SequencedTaskRunner>& processor_task_runner)
    : processor_(processor), processor_task_runner_(processor_task_runner) {}

ModelTypeProcessorProxy::~ModelTypeProcessorProxy() {}

void ModelTypeProcessorProxy::ConnectSync(scoped_ptr<CommitQueue> worker) {
  processor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ModelTypeProcessor::ConnectSync, processor_,
                            base::Passed(std::move(worker))));
}

void ModelTypeProcessorProxy::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  processor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ModelTypeProcessor::OnCommitCompleted, processor_,
                            type_state, response_list));
}

void ModelTypeProcessorProxy::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    const UpdateResponseDataList& updates) {
  processor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ModelTypeProcessor::OnUpdateReceived, processor_,
                            type_state, updates));
}

}  // namespace

SharedModelTypeProcessor::SharedModelTypeProcessor(syncer::ModelType type,
                                                   ModelTypeService* service)
    : type_(type),
      is_metadata_loaded_(false),
      service_(service),
      weak_ptr_factory_(this),
      weak_ptr_factory_for_sync_(this) {
  DCHECK(service);
}

SharedModelTypeProcessor::~SharedModelTypeProcessor() {}

void SharedModelTypeProcessor::OnSyncStarting(StartCallback start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(start_callback_.is_null());
  DCHECK(!IsConnected());
  DVLOG(1) << "Sync is starting for " << ModelTypeToString(type_);

  start_callback_ = start_callback;
  ConnectIfReady();
}

void SharedModelTypeProcessor::OnMetadataLoaded(
    scoped_ptr<MetadataBatch> batch) {
  DCHECK(CalledOnValidThread());
  DCHECK(entities_.empty());
  DCHECK(!is_metadata_loaded_);
  DCHECK(!IsConnected());

  // Flip this flag here to cover all cases where we don't need to load data.
  is_pending_commit_data_loaded_ = true;

  if (batch->GetDataTypeState().initial_sync_done()) {
    EntityMetadataMap metadata_map(batch->TakeAllMetadata());
    std::vector<std::string> entities_to_commit;

    for (auto it = metadata_map.begin(); it != metadata_map.end(); it++) {
      scoped_ptr<ProcessorEntityTracker> entity =
          ProcessorEntityTracker::CreateFromMetadata(it->first, &it->second);
      if (entity->RequiresCommitData()) {
        entities_to_commit.push_back(entity->client_tag());
      }
      entities_[entity->metadata().client_tag_hash()] = std::move(entity);
    }
    data_type_state_ = batch->GetDataTypeState();
    if (!entities_to_commit.empty()) {
      is_pending_commit_data_loaded_ = false;
      service_->GetData(entities_to_commit,
                        base::Bind(&SharedModelTypeProcessor::OnDataLoaded,
                                   weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    // First time syncing; initialize metadata.
    data_type_state_.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type_));
  }

  is_metadata_loaded_ = true;
  ConnectIfReady();
}

void SharedModelTypeProcessor::OnDataLoaded(syncer::SyncError error,
                                            scoped_ptr<DataBatch> data_batch) {
  while (data_batch->HasNext()) {
    TagAndData data = data_batch->Next();
    ProcessorEntityTracker* entity = GetEntityForTag(data.first);
    // If the entity wasn't deleted or updated with new commit.
    if (entity != nullptr && entity->RequiresCommitData()) {
      entity->CacheCommitData(data.second.get());
    }
  }
  is_pending_commit_data_loaded_ = true;
  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::ConnectIfReady() {
  DCHECK(CalledOnValidThread());
  if (!is_metadata_loaded_ || start_callback_.is_null()) {
    return;
  }

  scoped_ptr<ActivationContext> activation_context =
      make_scoped_ptr(new ActivationContext);
  activation_context->data_type_state = data_type_state_;
  activation_context->type_processor = make_scoped_ptr(
      new ModelTypeProcessorProxy(weak_ptr_factory_for_sync_.GetWeakPtr(),
                                  base::ThreadTaskRunnerHandle::Get()));

  start_callback_.Run(syncer::SyncError(), std::move(activation_context));
  start_callback_.Reset();
}

bool SharedModelTypeProcessor::IsAllowingChanges() const {
  return is_metadata_loaded_;
}

bool SharedModelTypeProcessor::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return !!worker_;
}

void SharedModelTypeProcessor::Disable() {
  DCHECK(CalledOnValidThread());
  scoped_ptr<MetadataChangeList> change_list =
      service_->CreateMetadataChangeList();
  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    change_list->ClearMetadata(it->second->client_tag());
  }
  change_list->ClearDataTypeState();
  // Nothing to do if this fails, so just ignore the error it might return.
  service_->ApplySyncChanges(std::move(change_list), EntityChangeList());

  // Destroy this object.
  // TODO(pavely): Revisit whether there's a better way to do this deletion.
  service_->clear_change_processor();
}

void SharedModelTypeProcessor::DisconnectSync() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for " << ModelTypeToString(type_);
  weak_ptr_factory_for_sync_.InvalidateWeakPtrs();
  worker_.reset();

  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    it->second->ClearTransientSyncState();
  }
}

base::WeakPtr<SharedModelTypeProcessor>
SharedModelTypeProcessor::AsWeakPtrForUI() {
  DCHECK(CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void SharedModelTypeProcessor::ConnectSync(scoped_ptr<CommitQueue> worker) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);

  worker_ = std::move(worker);

  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::Put(const std::string& tag,
                                   scoped_ptr<EntityData> data,
                                   MetadataChangeList* metadata_change_list) {
  DCHECK(IsAllowingChanges());
  DCHECK(data.get());
  DCHECK(!data->is_deleted());
  DCHECK(!data->non_unique_name.empty());
  DCHECK_EQ(type_, syncer::GetModelTypeFromSpecifics(data->specifics));

  if (!data_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  // Fill in some data.
  data->client_tag_hash = GetHashForTag(tag);
  if (data->modification_time.is_null()) {
    data->modification_time = base::Time::Now();
  }

  ProcessorEntityTracker* entity = GetEntityForTagHash(data->client_tag_hash);

  if (entity == nullptr) {
    // The service is creating a new entity.
    if (data->creation_time.is_null()) {
      data->creation_time = data->modification_time;
    }
    entity = CreateEntity(tag, *data);
  }

  // TODO(stanisc): crbug.com/561829: Avoid committing a change if there is no
  // actual change.
  entity->MakeLocalChange(std::move(data));
  metadata_change_list->UpdateMetadata(tag, entity->metadata());

  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::Delete(
    const std::string& tag,
    MetadataChangeList* metadata_change_list) {
  DCHECK(IsAllowingChanges());

  if (!data_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntityTracker* entity = GetEntityForTag(tag);
  if (entity == nullptr) {
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as deleted as far as the model is concerned.
    DLOG(WARNING) << "Attempted to delete missing item."
                  << " client tag: " << tag;
    return;
  }

  entity->Delete();

  metadata_change_list->UpdateMetadata(tag, entity->metadata());
  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::FlushPendingCommitRequests() {
  CommitRequestDataList commit_requests;

  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected())
    return;

  // Don't send anything if the type is not ready to handle commits.
  if (!data_type_state_.initial_sync_done())
    return;

  // Dont send anything if the initial data load is incomplete.
  if (!is_pending_commit_data_loaded_)
    return;

  // TODO(rlarocque): Do something smarter than iterate here.
  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    ProcessorEntityTracker* entity = it->second.get();
    if (entity->RequiresCommitRequest()) {
      DCHECK(!entity->RequiresCommitData());
      CommitRequestData request;
      entity->InitializeCommitRequestData(&request);
      commit_requests.push_back(request);
    }
  }

  if (!commit_requests.empty())
    worker_->EnqueueForCommit(commit_requests);
}

void SharedModelTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  scoped_ptr<MetadataChangeList> change_list =
      service_->CreateMetadataChangeList();

  data_type_state_ = type_state;
  change_list->UpdateDataTypeState(data_type_state_);

  for (const CommitResponseData& data : response_list) {
    ProcessorEntityTracker* entity = GetEntityForTagHash(data.client_tag_hash);
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for missing item."
                   << " type: " << type_
                   << " client_tag_hash: " << data.client_tag_hash;
      continue;
    }

    entity->ReceiveCommitResponse(data.id, data.sequence_number,
                                  data.response_version,
                                  data_type_state_.encryption_key_name());

    if (entity->CanClearMetadata()) {
      change_list->ClearMetadata(entity->client_tag());
      entities_.erase(entity->metadata().client_tag_hash());
    } else {
      change_list->UpdateMetadata(entity->client_tag(), entity->metadata());
    }
  }

  // TODO(stanisc): crbug.com/570085: Error handling.
  service_->ApplySyncChanges(std::move(change_list), EntityChangeList());
}

void SharedModelTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    const UpdateResponseDataList& updates) {
  if (!data_type_state_.initial_sync_done()) {
    OnInitialUpdateReceived(data_type_state, updates);
    return;
  }

  scoped_ptr<MetadataChangeList> metadata_changes =
      service_->CreateMetadataChangeList();
  EntityChangeList entity_changes;

  metadata_changes->UpdateDataTypeState(data_type_state);
  bool got_new_encryption_requirements =
      data_type_state_.encryption_key_name() !=
      data_type_state.encryption_key_name();
  data_type_state_ = data_type_state;

  for (const UpdateResponseData& update : updates) {
    const EntityData& data = update.entity.value();
    const std::string& client_tag_hash = data.client_tag_hash;

    ProcessorEntityTracker* entity = GetEntityForTagHash(client_tag_hash);
    if (entity == nullptr) {
      if (data.is_deleted()) {
        DLOG(WARNING) << "Received remote delete for a non-existing item."
                      << " client_tag_hash: " << client_tag_hash;
        continue;
      }

      entity = CreateEntity(data);
      entity_changes.push_back(
          EntityChange::CreateAdd(entity->client_tag(), update.entity));
    } else {
      if (data.is_deleted()) {
        entity_changes.push_back(
            EntityChange::CreateDelete(entity->client_tag()));
      } else {
        // TODO(stanisc): crbug.com/561829: Avoid sending an update to the
        // service if there is no actual change.
        entity_changes.push_back(
            EntityChange::CreateUpdate(entity->client_tag(), update.entity));
      }
    }

    entity->ApplyUpdateFromServer(update);

    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (data_type_state_.encryption_key_name() != update.encryption_key_name) {
      DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
               << update.encryption_key_name << " -> "
               << data_type_state_.encryption_key_name();
      auto it2 = entities_.find(client_tag_hash);
      it2->second->UpdateDesiredEncryptionKey(
          data_type_state_.encryption_key_name());
    }

    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->client_tag());
      entities_.erase(entity->metadata().client_tag_hash());
    } else {
      // TODO(stanisc): crbug.com/521867: Do something special when conflicts
      // are detected.
      metadata_changes->UpdateMetadata(entity->client_tag(),
                                       entity->metadata());
    }
  }

  if (got_new_encryption_requirements) {
    for (auto it = entities_.begin(); it != entities_.end(); ++it) {
      it->second->UpdateDesiredEncryptionKey(
          data_type_state_.encryption_key_name());
    }
  }

  // Inform the service of the new or updated data.
  // TODO(stanisc): crbug.com/570085: Error handling.
  service_->ApplySyncChanges(std::move(metadata_changes), entity_changes);

  // We may have new reasons to commit by the time this function is done.
  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    const UpdateResponseDataList& updates) {
  DCHECK(entities_.empty());
  // Ensure that initial sync was not already done and that the worker
  // correctly marked initial sync as done for this update.
  DCHECK(!data_type_state_.initial_sync_done());
  DCHECK(data_type_state.initial_sync_done());

  scoped_ptr<MetadataChangeList> metadata_changes =
      service_->CreateMetadataChangeList();
  EntityDataMap data_map;

  data_type_state_ = data_type_state;
  metadata_changes->UpdateDataTypeState(data_type_state_);

  for (const UpdateResponseData& update : updates) {
    ProcessorEntityTracker* entity = CreateEntity(update.entity.value());
    const std::string& tag = entity->client_tag();
    entity->ApplyUpdateFromServer(update);
    metadata_changes->UpdateMetadata(tag, entity->metadata());
    data_map[tag] = update.entity;
  }

  // Let the service handle associating and merging the data.
  // TODO(stanisc): crbug.com/570085: Error handling.
  service_->MergeSyncData(std::move(metadata_changes), data_map);

  // We may have new reasons to commit by the time this function is done.
  FlushPendingCommitRequests();
}

std::string SharedModelTypeProcessor::GetHashForTag(const std::string& tag) {
  return syncer::syncable::GenerateSyncableHash(type_, tag);
}

ProcessorEntityTracker* SharedModelTypeProcessor::GetEntityForTag(
    const std::string& tag) {
  return GetEntityForTagHash(GetHashForTag(tag));
}

ProcessorEntityTracker* SharedModelTypeProcessor::GetEntityForTagHash(
    const std::string& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

ProcessorEntityTracker* SharedModelTypeProcessor::CreateEntity(
    const std::string& tag,
    const EntityData& data) {
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  scoped_ptr<ProcessorEntityTracker> entity = ProcessorEntityTracker::CreateNew(
      tag, data.client_tag_hash, data.id, data.creation_time);
  ProcessorEntityTracker* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  return entity_ptr;
}

ProcessorEntityTracker* SharedModelTypeProcessor::CreateEntity(
    const EntityData& data) {
  // Let the service define |client_tag| based on the entity data.
  const std::string tag = service_->GetClientTag(data);
  // This constraint may be relaxed in the future.
  DCHECK_EQ(data.client_tag_hash, GetHashForTag(tag));
  return CreateEntity(tag, data);
}

}  // namespace syncer_v2
