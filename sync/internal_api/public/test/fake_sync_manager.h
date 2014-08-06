// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/test/null_sync_context_proxy.h"
#include "sync/internal_api/public/test/test_user_share.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace syncer {

class FakeSyncEncryptionHandler;

class FakeSyncManager : public SyncManager {
 public:
  // |initial_sync_ended_types|: The set of types that have initial_sync_ended
  // set to true. This value will be used by InitialSyncEndedTypes() until the
  // next configuration is performed.
  //
  // |progress_marker_types|: The set of types that have valid progress
  // markers. This will be used by GetTypesWithEmptyProgressMarkerToken() until
  // the next configuration is performed.
  //
  // |configure_fail_types|: The set of types that will fail
  // configuration. Once ConfigureSyncer is called, the
  // |initial_sync_ended_types_| and |progress_marker_types_| will be updated
  // to include those types that didn't fail.
  FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                  ModelTypeSet progress_marker_types,
                  ModelTypeSet configure_fail_types);
  virtual ~FakeSyncManager();

  // Returns those types that have been cleaned (purged from the directory)
  // since the last call to GetAndResetCleanedTypes(), or since startup if never
  // called.
  ModelTypeSet GetAndResetCleanedTypes();

  // Returns those types that have been downloaded since the last call to
  // GetAndResetDownloadedTypes(), or since startup if never called.
  ModelTypeSet GetAndResetDownloadedTypes();

  // Returns those types that have been marked as enabled since the
  // last call to GetAndResetEnabledTypes(), or since startup if never
  // called.
  ModelTypeSet GetAndResetEnabledTypes();

  // Returns the types that have most recently received a refresh request.
  ModelTypeSet GetLastRefreshRequestTypes();

  // Returns the most recent configuration reason since the last call to
  // GetAndResetConfigureReason, or since startup if never called.
  ConfigureReason GetAndResetConfigureReason();

  // Posts a method to invalidate the given IDs on the sync thread.
  virtual void OnIncomingInvalidation(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> interface) OVERRIDE;

  // Posts a method to update the invalidator state on the sync thread.
  virtual void SetInvalidatorEnabled(bool invalidator_enabled) OVERRIDE;

  // Block until the sync thread has finished processing any pending messages.
  void WaitForSyncThread();

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const GURL& service_url,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
      ExtensionsActivity* extensions_activity,
      ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      InternalComponentsFactory* internal_components_factory,
      Encryptor* encryptor,
      scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction report_unrecoverable_error_function,
      CancelationSignal* cancelation_signal) OVERRIDE;
  virtual ModelTypeSet InitialSyncEndedTypes() OVERRIDE;
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) OVERRIDE;
  virtual bool PurgePartiallySyncedTypes() OVERRIDE;
  virtual void UpdateCredentials(const SyncCredentials& credentials) OVERRIDE;
  virtual void StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) OVERRIDE;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet to_download,
      ModelTypeSet to_purge,
      ModelTypeSet to_journal,
      ModelTypeSet to_unapply,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual SyncStatus GetDetailedStatus() const OVERRIDE;
  virtual void SaveChanges() OVERRIDE;
  virtual void ShutdownOnSyncThread(ShutdownReason reason) OVERRIDE;
  virtual UserShare* GetUserShare() OVERRIDE;
  virtual syncer::SyncContextProxy* GetSyncContextProxy() OVERRIDE;
  virtual const std::string cache_guid() OVERRIDE;
  virtual bool ReceivedExperiment(Experiments* experiments) OVERRIDE;
  virtual bool HasUnsyncedItems() OVERRIDE;
  virtual SyncEncryptionHandler* GetEncryptionHandler() OVERRIDE;
  virtual ScopedVector<syncer::ProtocolEvent>
      GetBufferedProtocolEvents() OVERRIDE;
  virtual scoped_ptr<base::ListValue> GetAllNodesForType(
      syncer::ModelType type) OVERRIDE;
  virtual void RefreshTypes(ModelTypeSet types) OVERRIDE;
  virtual void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual void RequestEmitDebugInfo() OVERRIDE;

 private:
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  ObserverList<SyncManager::Observer> observers_;

  // Faked directory state.
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;

  // Test specific state.
  // The types that should fail configuration attempts. These types will not
  // have their progress markers or initial_sync_ended bits set.
  ModelTypeSet configure_fail_types_;
  // The set of types that have been cleaned up.
  ModelTypeSet cleaned_types_;
  // The set of types that have been downloaded.
  ModelTypeSet downloaded_types_;
  // The set of types that have been enabled.
  ModelTypeSet enabled_types_;

  // The types for which a refresh was most recently requested.
  ModelTypeSet last_refresh_request_types_;

  // The most recent configure reason.
  ConfigureReason last_configure_reason_;

  scoped_ptr<FakeSyncEncryptionHandler> fake_encryption_handler_;

  TestUserShare test_user_share_;

  NullSyncContextProxy null_sync_context_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
