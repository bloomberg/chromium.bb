// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_
#define SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
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
class ModelTypeEntity;

// A sync component embedded on the synced type's thread that helps to handle
// communication between sync and model type threads.
class SYNC_EXPORT SharedModelTypeProcessor : public ModelTypeProcessor,
                                             public ModelTypeChangeProcessor,
                                             base::NonThreadSafe {
 public:
  SharedModelTypeProcessor(syncer::ModelType type, ModelTypeService* service);
  ~SharedModelTypeProcessor() override;

  typedef base::Callback<void(syncer::SyncError, scoped_ptr<ActivationContext>)>
      StartCallback;

  // Called by the DataTypeController to gather additional information needed
  // before a CommitQueue object can be created for this model type. Once the
  // metadata has been loaded, the info is collected and given to |callback|.
  // Once called, this can only be called again if sync is disconnected.
  void OnSyncStarting(StartCallback callback);

  // Disconnect this processor from the sync engine. Change metadata will
  // continue being processed and persisted, but no commits can be made until
  // the next time sync is connected.
  void DisconnectSync();

  // Indicates that we no longer want to do any sync-related things for this
  // data type. Severs all ties to the sync thread, deletes all local sync
  // metadata, and then destroys the SharedModelTypeProcessor.
  // TODO(crbug.com/584365): This needs to be called from DataTypeController.
  void Disable();

  // Whether the processor is allowing changes to its model type. If this is
  // false, the service should not allow any changes to its data.
  bool IsAllowingChanges() const;

  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // ModelTypeChangeProcessor implementation.
  void Put(const std::string& client_tag,
           scoped_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void OnMetadataLoaded(scoped_ptr<MetadataBatch> batch) override;

  // Returns the long-lived WeakPtr that is intended to be registered with the
  // ProfileSyncService.
  base::WeakPtr<SharedModelTypeProcessor> AsWeakPtrForUI();

  // ModelTypeProcessor implementation.
  void ConnectSync(scoped_ptr<CommitQueue> worker) override;
  void OnCommitCompleted(const sync_pb::DataTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::DataTypeState& type_state,
                        const UpdateResponseDataList& updates) override;

 private:
  friend class SharedModelTypeProcessorTest;

  using EntityMap = std::map<std::string, scoped_ptr<ModelTypeEntity>>;
  using UpdateMap = std::map<std::string, scoped_ptr<UpdateResponseData>>;

  // Callback for ModelTypeService::GetData(). Used when we need to load data
  // for pending commits during the initialization process.
  void OnDataLoaded(syncer::SyncError error, scoped_ptr<DataBatch> data_batch);

  // Check conditions, and if met inform sync that we are ready to connect.
  void ConnectIfReady();

  // Handle the first update received from the server after being enabled.
  void OnInitialUpdateReceived(const sync_pb::DataTypeState& type_state,
                               const UpdateResponseDataList& updates);

  // Sends all commit requests that are due to be sent to the sync thread.
  void FlushPendingCommitRequests();

  // Computes the client tag hash for the given client |tag|.
  std::string GetHashForTag(const std::string& tag);

  // Gets the entity for the given tag, or null if there isn't one.
  ModelTypeEntity* GetEntityForTag(const std::string& tag);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ModelTypeEntity* GetEntityForTagHash(const std::string& tag_hash);

  // Create an entity in the entity map for |tag| and return a pointer to it.
  // Requires that no entity for |tag| already exists in the map.
  ModelTypeEntity* CreateEntity(const std::string& tag, const EntityData& data);

  // Version of the above that generates a tag for |data|.
  ModelTypeEntity* CreateEntity(const EntityData& data);

  syncer::ModelType type_;
  sync_pb::DataTypeState data_type_state_;

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  StartCallback start_callback_;

  // Indicates whether the metadata has finished loading.
  bool is_metadata_loaded_;

  // Indicates whether data for pending commits has finished loading.
  bool is_pending_commit_data_loaded_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  scoped_ptr<CommitQueue> worker_;

  // The set of sync entities known to this object.
  EntityMap entities_;

  // ModelTypeService linked to this processor.
  // The service owns this processor instance so the pointer should never
  // become invalid.
  ModelTypeService* const service_;

  // We use two different WeakPtrFactories because we want the pointers they
  // issue to have different lifetimes.  When asked to disconnect from the sync
  // thread, we want to make sure that no tasks generated as part of the
  // now-obsolete connection to affect us.  But we also want the WeakPtr we
  // sent to the UI thread to remain valid.
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_;
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_for_sync_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_
