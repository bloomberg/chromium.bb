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
#include "sync/api/metadata_batch.h"
#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_change_processor.h"
#include "sync/api/model_type_service.h"
#include "sync/api/sync_error.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/model_type_processor.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
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

  // Called by DataTypeController to begins asynchronous operation of preparing
  // the model to sync. Once the model is ready to be activated with Sync the
  // callback will be invoked with the activation context. If the model is
  // already ready it is safe to call the callback right away. Otherwise the
  // callback needs to be stored and called when the model is ready.
  void Start(StartCallback callback);

  // Called by DataTypeController to inform the model that the sync is
  // stopping for the model type.
  void Stop();

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

  // Returns the list of pending updates.
  //
  // This is used as a helper function, but it's public mainly for testing.
  // The current test harness setup doesn't allow us to test the data that the
  // proxy sends to the worker during initialization, so we use this to inspect
  // its state instead.
  UpdateResponseDataList GetPendingUpdates();

  // Returns the long-lived WeakPtr that is intended to be registered with the
  // ProfileSyncService.
  base::WeakPtr<SharedModelTypeProcessor> AsWeakPtrForUI();

  // ModelTypeProcessor implementation.
  void OnConnect(scoped_ptr<CommitQueue> worker) override;
  void OnCommitCompleted(const DataTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const DataTypeState& type_state,
                        const UpdateResponseDataList& response_list,
                        const UpdateResponseDataList& pending_updates) override;

 private:
  friend class SharedModelTypeProcessorTest;

  using EntityMap = std::map<std::string, scoped_ptr<ModelTypeEntity>>;
  using UpdateMap = std::map<std::string, scoped_ptr<UpdateResponseData>>;

  // Complete the start process.
  void FinishStart();

  // Handle the first update received from the server after being enabled.
  void OnInitialUpdateReceived(const DataTypeState& type_state,
                               const UpdateResponseDataList& response_list,
                               const UpdateResponseDataList& pending_updates);

  // Sends all commit requests that are due to be sent to the sync thread.
  void FlushPendingCommitRequests();

  // Clears any state related to outstanding communications with the
  // CommitQueue.  Used when we want to disconnect from
  // the current worker.
  void ClearTransientSyncState();

  syncer::ModelType type_;
  DataTypeState data_type_state_;

  // Stores the start callback in between Start() and FinishStart().
  StartCallback start_callback_;

  // Indicates whether the metadata has finished loading.
  bool is_metadata_loaded_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  scoped_ptr<CommitQueue> worker_;

  // The set of sync entities known to this object.
  EntityMap entities_;

  // A set of updates that can not be applied at this time.  These are never
  // used by the model.  They are kept here only so we can save and restore
  // them across restarts, and keep them in sync with our progress markers.
  UpdateMap pending_updates_map_;

  // ModelTypeService linked to this processor.
  // The service owns this processor instance so the pointer should never
  // become invalid.
  ModelTypeService* const service_;

  // We use two different WeakPtrFactories because we want the pointers they
  // issue to have different lifetimes.  When asked to disconnect from the sync
  // thread, we want to make sure that no tasks generated as part of the
  // now-obsolete connection to affect us.  But we also want the WeakPtr we
  // sent to the UI thread to remain valid.
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_for_ui_;
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_for_sync_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SHARED_MODEL_TYPE_PROCESSOR_H_
