// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_
#define SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_

#include <string>
#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/syncable/transaction_observer.h"

namespace syncer {

class WriteTransaction;

// Base class of sync managers used for backup and rollback. Two major
// functions are:
//   * Init(): load backup DB into sync directory.
//   * ConfigureSyncer(): initialize permanent sync nodes (root, bookmark
//                        permanent folders) for configured type as needed.
//
// Most of other functions are no ops.
class SYNC_EXPORT_PRIVATE SyncRollbackManagerBase :
    public SyncManager,
    public syncable::DirectoryChangeDelegate,
    public syncable::TransactionObserver {
 public:
  SyncRollbackManagerBase();
  virtual ~SyncRollbackManagerBase();

  // SyncManager implementation.
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
      ExtensionsActivity* extensions_activity,
      SyncManager::ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      InternalComponentsFactory* internal_components_factory,
      Encryptor* encryptor,
      scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      CancelationSignal* cancelation_signal) OVERRIDE;
  virtual ModelTypeSet InitialSyncEndedTypes() OVERRIDE;
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) OVERRIDE;
  virtual bool PurgePartiallySyncedTypes() OVERRIDE;
  virtual void UpdateCredentials(const SyncCredentials& credentials) OVERRIDE;
  virtual void StartSyncingNormally(const ModelSafeRoutingInfo& routing_info)
      OVERRIDE;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet to_download,
      ModelTypeSet to_purge,
      ModelTypeSet to_journal,
      ModelTypeSet to_unapply,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) OVERRIDE;
  virtual void SetInvalidatorEnabled(bool invalidator_enabled) OVERRIDE;
  virtual void OnIncomingInvalidation(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> invalidation) OVERRIDE;
  virtual void AddObserver(SyncManager::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(SyncManager::Observer* observer) OVERRIDE;
  virtual SyncStatus GetDetailedStatus() const OVERRIDE;
  virtual void SaveChanges() OVERRIDE;
  virtual void ShutdownOnSyncThread() OVERRIDE;
  virtual UserShare* GetUserShare() OVERRIDE;
  virtual const std::string cache_guid() OVERRIDE;
  virtual bool ReceivedExperiment(Experiments* experiments) OVERRIDE;
  virtual bool HasUnsyncedItems() OVERRIDE;
  virtual SyncEncryptionHandler* GetEncryptionHandler() OVERRIDE;
  virtual void RefreshTypes(ModelTypeSet types) OVERRIDE;
  virtual SyncContextProxy* GetSyncContextProxy() OVERRIDE;
  virtual ScopedVector<ProtocolEvent> GetBufferedProtocolEvents()
      OVERRIDE;
  virtual scoped_ptr<base::ListValue> GetAllNodesForType(
      syncer::ModelType type) OVERRIDE;

  // DirectoryChangeDelegate implementation.
  virtual void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) OVERRIDE;
  virtual ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) OVERRIDE;

  // syncable::TransactionObserver implementation.
  virtual void OnTransactionWrite(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) OVERRIDE;

 protected:
  ObserverList<SyncManager::Observer>* GetObservers();

  virtual void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) OVERRIDE;
  virtual void RequestEmitDebugInfo() OVERRIDE;

  bool initialized() const {
    return initialized_;
  }

 private:
  void NotifyInitializationSuccess();
  void NotifyInitializationFailure();

  bool InitBackupDB(
      const base::FilePath& sync_folder,
      InternalComponentsFactory* internal_components_factory);

  bool InitTypeRootNode(ModelType type);
  void InitBookmarkFolder(const std::string& folder);

  UserShare share_;
  ObserverList<SyncManager::Observer> observers_;

  scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler_;
  ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;

  base::WeakPtrFactory<SyncRollbackManagerBase> weak_ptr_factory_;

  scoped_ptr<SyncEncryptionHandler> dummy_handler_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(SyncRollbackManagerBase);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_
