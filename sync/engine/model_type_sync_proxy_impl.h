// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_IMPL_H_
#define SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/non_thread_safe.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

class SyncContextProxy;
class ModelTypeEntity;
class ModelTypeSyncWorker;

// A sync component embedded on the synced type's thread that helps to handle
// communication between sync and model type threads.
class SYNC_EXPORT_PRIVATE ModelTypeSyncProxyImpl : base::NonThreadSafe {
 public:
  ModelTypeSyncProxyImpl(ModelType type);
  virtual ~ModelTypeSyncProxyImpl();

  // Returns true if this object believes that sync is preferred for this type.
  //
  // By "preferred", we mean that a policy decision has been made that this
  // type should be synced.  Most of the time this is controlled by a user
  // clicking a checkbox on the settings page.
  //
  // The canonical preferred state is based on SyncPrefs on the UI thread.  At
  // best, this value is stale and may lag behind the one set on the UI thread.
  // Before this type has registered with the UI thread, it's mostly just an
  // informed guess.
  bool IsPreferred() const;

  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // Returns the model type handled by this type sync proxy.
  ModelType GetModelType() const;

  // Starts the handshake with the sync thread.
  void Enable(scoped_ptr<SyncContextProxy> context_proxy);

  // Severs all ties to the sync thread and may delete local sync state.
  // Another call to Enable() can be used to re-establish this connection.
  void Disable();

  // Severs all ties to the sync thread.
  // Another call to Enable() can be used to re-establish this connection.
  void Disconnect();

  // Callback used to process the handshake response.
  void OnConnect(scoped_ptr<ModelTypeSyncWorker> worker);

  // Requests that an item be stored in sync.
  void Put(const std::string& client_tag,
           const sync_pb::EntitySpecifics& specifics);

  // Deletes an item from sync.
  void Delete(const std::string& client_tag);

  // Informs this object that some of its commit requests have been
  // successfully serviced.
  void OnCommitCompleted(const DataTypeState& type_state,
                         const CommitResponseDataList& response_list);

  // Informs this object that there are some incoming updates is should
  // handle.
  void OnUpdateReceived(const DataTypeState& type_state,
                        const UpdateResponseDataList& response_list,
                        const UpdateResponseDataList& pending_updates);

  // Returns the list of pending updates.
  //
  // This is used as a helper function, but it's public mainly for testing.
  // The current test harness setup doesn't allow us to test the data that the
  // proxy sends to the worker during initialization, so we use this to inspect
  // its state instead.
  UpdateResponseDataList GetPendingUpdates();

  // Returns the long-lived WeakPtr that is intended to be registered with the
  // ProfileSyncService.
  base::WeakPtr<ModelTypeSyncProxyImpl> AsWeakPtrForUI();

 private:
  typedef std::map<std::string, ModelTypeEntity*> EntityMap;
  typedef std::map<std::string, UpdateResponseData*> UpdateMap;

  // Sends all commit requests that are due to be sent to the sync thread.
  void FlushPendingCommitRequests();

  // Clears any state related to outstanding communications with the
  // ModelTypeSyncWorker.  Used when we want to disconnect from
  // the current worker.
  void ClearTransientSyncState();

  // Clears any state related to our communications with the current sync
  // account.  Useful when a user signs out of the current account.
  void ClearSyncState();

  ModelType type_;
  DataTypeState data_type_state_;

  // Whether or not sync is preferred for this type.  This is a cached copy of
  // the canonical copy information on the UI thread.
  bool is_preferred_;

  // Whether or not this object has completed its initial handshake with the
  // SyncContextProxy.
  bool is_connected_;

  // Our link to data type management on the sync thread.
  // Used for enabling and disabling sync for this type.
  //
  // Beware of NULL pointers: This object is uninitialized when we are not
  // connected to sync.
  scoped_ptr<SyncContextProxy> sync_context_proxy_;

  // Reference to the ModelTypeSyncWorker.
  //
  // The interface hides the posting of tasks across threads as well as the
  // ModelTypeSyncWorker's implementation.  Both of these features are
  // useful in tests.
  scoped_ptr<ModelTypeSyncWorker> worker_;

  // The set of sync entities known to this object.
  EntityMap entities_;
  STLValueDeleter<EntityMap> entities_deleter_;

  // A set of updates that can not be applied at this time.  These are never
  // used by the model.  They are kept here only so we can save and restore
  // them across restarts, and keep them in sync with our progress markers.
  UpdateMap pending_updates_map_;
  STLValueDeleter<UpdateMap> pending_updates_map_deleter_;

  // We use two different WeakPtrFactories because we want the pointers they
  // issue to have different lifetimes.  When asked to disconnect from the sync
  // thread, we want to make sure that no tasks generated as part of the
  // now-obsolete connection to affect us.  But we also want the WeakPtr we
  // sent to the UI thread to remain valid.
  base::WeakPtrFactory<ModelTypeSyncProxyImpl> weak_ptr_factory_for_ui_;
  base::WeakPtrFactory<ModelTypeSyncProxyImpl> weak_ptr_factory_for_sync_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_IMPL_H_
