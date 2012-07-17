// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_MANAGER_H_

#include <string>
#include <vector>

#include "sync/internal_api/public/sync_manager.h"

namespace syncer {

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SyncManagerImpl : public SyncManager {
 public:
  // SyncInternal contains the implementation of SyncManager, while abstracting
  // internal types from clients of the interface.
  class SyncInternal;

  // Create an uninitialized SyncManager.  Callers must Init() before using.
  explicit SyncManagerImpl(const std::string& name);
  virtual ~SyncManagerImpl();

  // SyncManager implementation.
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
      syncer::ExtensionsActivityMonitor* extensions_activity_monitor,
      SyncManager::ChangeDelegate* change_delegate,
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
  virtual void UpdateEnabledTypes(
      const syncer::ModelTypeSet& enabled_types) OVERRIDE;
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

  // Functions used for testing.

  // Returns true if we are currently encrypting all sync data.  May
  // be called on any thread.
  bool EncryptEverythingEnabledForTest() const;

  // Gets the set of encrypted types from the cryptographer
  // Note: opens a transaction.  May be called from any thread.
  syncer::ModelTypeSet GetEncryptedDataTypesForTest() const;

  void SimulateEnableNotificationsForTest();
  void SimulateDisableNotificationsForTest(int reason);
  void TriggerOnIncomingNotificationForTest(syncer::ModelTypeSet model_types);

  static int GetDefaultNudgeDelay();
  static int GetPreferencesNudgeDelay();

 private:
  friend class SyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, NudgeDelayTest);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, OnNotificationStateChange);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, OnIncomingNotification);

  base::TimeDelta GetNudgeDelayTimeDelta(const syncer::ModelType& model_type);

  // Set the internal scheduler for testing purposes.
  // TODO(sync): Use dependency injection instead. crbug.com/133061
  void SetSyncSchedulerForTest(
      scoped_ptr<syncer::SyncScheduler> scheduler);

  base::ThreadChecker thread_checker_;

  // An opaque pointer to the nested private class.
  SyncInternal* data_;

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_MANAGER_H_
