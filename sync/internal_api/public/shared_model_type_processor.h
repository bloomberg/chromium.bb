// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_
#define SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/data_batch.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_change_processor.h"
#include "sync/api/model_type_service.h"
#include "sync/api/sync_error.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/model_type_processor.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {
struct ActivationContext;
class CommitQueue;
class ProcessorEntityTracker;

// A sync component embedded on the synced type's thread that helps to handle
// communication between sync and model type threads.
class SYNC_EXPORT SharedModelTypeProcessor : public ModelTypeProcessor,
                                             public ModelTypeChangeProcessor,
                                             base::NonThreadSafe {
 public:
  SharedModelTypeProcessor(syncer::ModelType type, ModelTypeService* service);
  ~SharedModelTypeProcessor() override;

  // An easily bound function that constructs a SharedModelTypeProcessor.
  static std::unique_ptr<ModelTypeChangeProcessor> CreateAsChangeProcessor(
      syncer::ModelType type,
      ModelTypeService* service);

  // Whether the processor is allowing changes to its model type. If this is
  // false, the service should not allow any changes to its data.
  bool IsAllowingChanges() const;

  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // ModelTypeChangeProcessor implementation.
  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void OnMetadataLoaded(std::unique_ptr<MetadataBatch> batch) override;
  void OnSyncStarting(const StartCallback& callback) override;
  void DisableSync() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void OnCommitCompleted(const sync_pb::DataTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::DataTypeState& type_state,
                        const UpdateResponseDataList& updates) override;

 private:
  friend class SharedModelTypeProcessorTest;

  using EntityMap =
      std::map<std::string, std::unique_ptr<ProcessorEntityTracker>>;
  using UpdateMap = std::map<std::string, std::unique_ptr<UpdateResponseData>>;

  // Check conditions, and if met inform sync that we are ready to connect.
  void ConnectIfReady();

  // Helper function to process the update for a single entity. If a local data
  // change is required, it will be added to |entity_changes|. The return value
  // is the tracker for this entity, or nullptr if the update should be ignored.
  ProcessorEntityTracker* ProcessUpdate(const UpdateResponseData& update,
                                        EntityChangeList* entity_changes);

  // Resolve a conflict between |update| and the pending commit in |entity|.
  ConflictResolution::Type ResolveConflict(const UpdateResponseData& update,
                                           ProcessorEntityTracker* entity,
                                           EntityChangeList* changes);

  // Recommit all entities for encryption except those in |already_updated|.
  void RecommitAllForEncryption(std::unordered_set<std::string> already_updated,
                                MetadataChangeList* metadata_changes);

  // Handle the first update received from the server after being enabled.
  void OnInitialUpdateReceived(const sync_pb::DataTypeState& type_state,
                               const UpdateResponseDataList& updates);

  // ModelTypeService::GetData() callback for initial pending commit data.
  void OnInitialPendingDataLoaded(syncer::SyncError error,
                                  std::unique_ptr<DataBatch> data_batch);

  // ModelTypeService::GetData() callback for re-encryption commit data.
  void OnDataLoadedForReEncryption(syncer::SyncError error,
                                   std::unique_ptr<DataBatch> data_batch);

  // Caches EntityData from the |data_batch| in the entity trackers.
  void ConsumeDataBatch(std::unique_ptr<DataBatch> data_batch);

  // Sends all commit requests that are due to be sent to the sync thread.
  void FlushPendingCommitRequests();

  // Computes the client tag hash for the given client |tag|.
  std::string GetHashForTag(const std::string& tag);

  // Gets the entity for the given tag, or null if there isn't one.
  ProcessorEntityTracker* GetEntityForTag(const std::string& tag);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntityTracker* GetEntityForTagHash(const std::string& tag_hash);

  // Create an entity in the entity map for |tag| and return a pointer to it.
  // Requires that no entity for |tag| already exists in the map.
  ProcessorEntityTracker* CreateEntity(const std::string& tag,
                                       const EntityData& data);

  // Version of the above that generates a tag for |data|.
  ProcessorEntityTracker* CreateEntity(const EntityData& data);

  syncer::ModelType type_;
  sync_pb::DataTypeState data_type_state_;

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  StartCallback start_callback_;

  // Indicates whether the metadata has finished loading.
  bool is_metadata_loaded_;

  // Indicates whether data for any initial pending commits has been loaded.
  bool is_initial_pending_data_loaded_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<CommitQueue> worker_;

  // The set of sync entities known to this object.
  EntityMap entities_;

  // ModelTypeService linked to this processor.
  // The service owns this processor instance so the pointer should never
  // become invalid.
  ModelTypeService* const service_;

  // WeakPtrFactory for this processor which will be sent to sync thread.
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_
