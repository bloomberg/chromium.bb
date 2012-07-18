// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_

#include <string>

#include "sync/internal_api/public/sync_manager.h"

#include "base/observer_list.h"

class MessageLoop;

namespace syncer {

class FakeSyncManager : public SyncManager {
 public:
  explicit FakeSyncManager();
  virtual ~FakeSyncManager();

  // The set of types that have initial_sync_ended set to true. This value will
  // be used by InitialSyncEndedTypes() until the next configuration is
  // performed.
  void set_initial_sync_ended_types(syncer::ModelTypeSet types);

  // The set of types that have valid progress markers. This will be used by
  // GetTypesWithEmptyProgressMarkerToken() until the next configuration is
  // performed.
  void set_progress_marker_types(syncer::ModelTypeSet types);

  // The set of types that will fail configuration. Once ConfigureSyncer is
  // called, the |initial_sync_ended_types_| and
  // |progress_marker_types_| will be updated to include those types
  // that didn't fail.
  void set_configure_fail_types(syncer::ModelTypeSet types);

  // Returns those types that have been cleaned (purged from the directory)
  // since the last call to GetAndResetCleanedTypes(), or since startup if never
  // called.
  syncer::ModelTypeSet GetAndResetCleanedTypes();

  // Returns those types that have been downloaded since the last call to
  // GetAndResetDownloadedTypes(), or since startup if never called.
  syncer::ModelTypeSet GetAndResetDownloadedTypes();

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  virtual bool Init(
      const FilePath& database_location,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      const scoped_refptr<base::TaskRunner>& blocking_task_runner,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const syncer::ModelSafeRoutingInfo& model_safe_routing_info,
      const std::vector<syncer::ModelSafeWorker*>& workers,
      syncer::ExtensionsActivityMonitor*
          extensions_activity_monitor,
      ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      scoped_ptr<syncer::SyncNotifier> sync_notifier,
      const std::string& restored_key_for_bootstrapping,
      TestingMode testing_mode,
      syncer::Encryptor* encryptor,
      syncer::UnrecoverableErrorHandler* unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function) OVERRIDE;
  virtual void ThrowUnrecoverableError() OVERRIDE;
  virtual syncer::ModelTypeSet InitialSyncEndedTypes() OVERRIDE;
  virtual syncer::ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      syncer::ModelTypeSet types) OVERRIDE;
  virtual bool PurgePartiallySyncedTypes() OVERRIDE;
  virtual void UpdateCredentials(const SyncCredentials& credentials) OVERRIDE;
  virtual void UpdateEnabledTypes(const syncer::ModelTypeSet& types) OVERRIDE;
  virtual void StartSyncingNormally(
      const syncer::ModelSafeRoutingInfo& routing_info) OVERRIDE;
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       bool is_explicit) OVERRIDE;
  virtual void SetDecryptionPassphrase(const std::string& passphrase) OVERRIDE;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      const syncer::ModelTypeSet& types_to_config,
      const syncer::ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual SyncStatus GetDetailedStatus() const OVERRIDE;
  virtual bool IsUsingExplicitPassphrase() OVERRIDE;
  virtual void SaveChanges() OVERRIDE;
  virtual void StopSyncingForShutdown(const base::Closure& callback) OVERRIDE;
  virtual void ShutdownOnSyncThread() OVERRIDE;
  virtual UserShare* GetUserShare() const OVERRIDE;
  virtual void RefreshNigori(const std::string& chrome_version,
                             const base::Closure& done_callback) OVERRIDE;
  virtual void EnableEncryptEverything() OVERRIDE;
  virtual bool ReceivedExperiment(
      syncer::Experiments* experiments) const OVERRIDE;
  virtual bool HasUnsyncedItems() const OVERRIDE;

 private:
  ObserverList<SyncManager::Observer> observers_;

  // Faked directory state.
  syncer::ModelTypeSet initial_sync_ended_types_;
  syncer::ModelTypeSet progress_marker_types_;

  // Test specific state.
  // The types that should fail configuration attempts. These types will not
  // have their progress markers or initial_sync_ended bits set.
  syncer::ModelTypeSet configure_fail_types_;
  // The set of types that have been cleaned up.
  syncer::ModelTypeSet cleaned_types_;
  // The set of types that have been downloaded.
  syncer::ModelTypeSet downloaded_types_;

  // For StopSyncingForShutdown's callback.
  MessageLoop* sync_loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
