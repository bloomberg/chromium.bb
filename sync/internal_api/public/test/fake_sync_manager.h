// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/notifier/invalidator_registrar.h"

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

  // Posts a method to invalidate the given IDs on the sync thread.
  void Invalidate(const ObjectIdInvalidationMap& invalidation_map);

  // Posts a method to update the invalidator state on the sync thread.
  void UpdateInvalidatorState(InvalidatorState state);

  // Block until the sync thread has finished processing any pending messages.
  void WaitForSyncThread();

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<ModelSafeWorker*>& workers,
      ExtensionsActivityMonitor* extensions_activity_monitor,
      ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      scoped_ptr<Invalidator> invalidator,
      const std::string& invalidator_client_id,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      scoped_ptr<InternalComponentsFactory> internal_components_factory,
      Encryptor* encryptor,
      UnrecoverableErrorHandler* unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function) OVERRIDE;
  virtual void ThrowUnrecoverableError() OVERRIDE;
  virtual ModelTypeSet InitialSyncEndedTypes() OVERRIDE;
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) OVERRIDE;
  virtual bool PurgePartiallySyncedTypes() OVERRIDE;
  virtual void UpdateCredentials(const SyncCredentials& credentials) OVERRIDE;
  virtual void UpdateEnabledTypes(ModelTypeSet types) OVERRIDE;
  virtual void RegisterInvalidationHandler(
      InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      InvalidationHandler* handler,
      const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      InvalidationHandler* handler) OVERRIDE;
  virtual void AcknowledgeInvalidation(
      const invalidation::ObjectId& id,
      const syncer::AckHandle& ack_handle) OVERRIDE;
  virtual void StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) OVERRIDE;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet types_to_config,
      ModelTypeSet failed_types,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual SyncStatus GetDetailedStatus() const OVERRIDE;
  virtual void SaveChanges() OVERRIDE;
  virtual void StopSyncingForShutdown(const base::Closure& callback) OVERRIDE;
  virtual void ShutdownOnSyncThread() OVERRIDE;
  virtual UserShare* GetUserShare() OVERRIDE;
  virtual const std::string cache_guid() OVERRIDE;
  virtual bool ReceivedExperiment(Experiments* experiments) OVERRIDE;
  virtual bool HasUnsyncedItems() OVERRIDE;
  virtual SyncEncryptionHandler* GetEncryptionHandler() OVERRIDE;
  virtual void RefreshTypes(ModelTypeSet types) OVERRIDE;

 private:
  void InvalidateOnSyncThread(
      const ObjectIdInvalidationMap& invalidation_map);
  void UpdateInvalidatorStateOnSyncThread(InvalidatorState state);

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

  // Faked invalidator state.
  InvalidatorRegistrar registrar_;

  // The types for which a refresh was most recently requested.
  ModelTypeSet last_refresh_request_types_;

  scoped_ptr<FakeSyncEncryptionHandler> fake_encryption_handler_;

  TestUserShare test_user_share_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
