// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_
#define SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_

#include <string>
#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
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
  virtual ModelTypeSet InitialSyncEndedTypes() override;
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) override;
  virtual bool PurgePartiallySyncedTypes() override;
  virtual void UpdateCredentials(const SyncCredentials& credentials) override;
  virtual void StartSyncingNormally(const ModelSafeRoutingInfo& routing_info)
      override;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet to_download,
      ModelTypeSet to_purge,
      ModelTypeSet to_journal,
      ModelTypeSet to_unapply,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) override;
  virtual void SetInvalidatorEnabled(bool invalidator_enabled) override;
  virtual void OnIncomingInvalidation(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> invalidation) override;
  virtual void AddObserver(SyncManager::Observer* observer) override;
  virtual void RemoveObserver(SyncManager::Observer* observer) override;
  virtual SyncStatus GetDetailedStatus() const override;
  virtual void SaveChanges() override;
  virtual void ShutdownOnSyncThread(ShutdownReason reason) override;
  virtual UserShare* GetUserShare() override;
  virtual const std::string cache_guid() override;
  virtual bool ReceivedExperiment(Experiments* experiments) override;
  virtual bool HasUnsyncedItems() override;
  virtual SyncEncryptionHandler* GetEncryptionHandler() override;
  virtual void RefreshTypes(ModelTypeSet types) override;
  virtual SyncContextProxy* GetSyncContextProxy() override;
  virtual ScopedVector<ProtocolEvent> GetBufferedProtocolEvents()
      override;
  virtual scoped_ptr<base::ListValue> GetAllNodesForType(
      syncer::ModelType type) override;

  // DirectoryChangeDelegate implementation.
  virtual void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) override;
  virtual ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) override;
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) override;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) override;

  // syncable::TransactionObserver implementation.
  virtual void OnTransactionWrite(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) override;

 protected:
  ObserverList<SyncManager::Observer>* GetObservers();

  // Initialize sync backup DB.
  bool InitInternal(
      const base::FilePath& database_location,
      InternalComponentsFactory* internal_components_factory,
      InternalComponentsFactory::StorageOption storage,
      scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction report_unrecoverable_error_function);

  virtual void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  virtual void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  virtual bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  virtual void RequestEmitDebugInfo() override;

  bool initialized() const {
    return initialized_;
  }

 private:
  void NotifyInitializationSuccess();
  void NotifyInitializationFailure();

  bool InitBackupDB(const base::FilePath& sync_folder,
                    InternalComponentsFactory* internal_components_factory,
                    InternalComponentsFactory::StorageOption storage);

  bool InitTypeRootNode(ModelType type);
  void InitBookmarkFolder(const std::string& folder);

  UserShare share_;
  ObserverList<SyncManager::Observer> observers_;

  scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler_;
  ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;

  scoped_ptr<SyncEncryptionHandler> dummy_handler_;

  bool initialized_;

  base::WeakPtrFactory<SyncRollbackManagerBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncRollbackManagerBase);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_BASE_H_
