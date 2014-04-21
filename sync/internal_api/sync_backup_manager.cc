// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_backup_manager.h"

#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"

namespace syncer {

SyncBackupManager::SyncBackupManager()
    : in_normalization_(false) {
}

SyncBackupManager::~SyncBackupManager() {
}

void SyncBackupManager::Init(
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
      CancelationSignal* cancelation_signal) {
  SyncRollbackManagerBase::Init(database_location, event_handler,
                                sync_server_and_path, sync_server_port,
                                use_ssl, post_factory.Pass(),
                                workers, extensions_activity, change_delegate,
                                credentials, invalidator_client_id,
                                restored_key_for_bootstrapping,
                                restored_keystore_key_for_bootstrapping,
                                internal_components_factory, encryptor,
                                unrecoverable_error_handler.Pass(),
                                report_unrecoverable_error_function,
                                cancelation_signal);

  GetUserShare()->directory->CollectMetaHandleCounts(
      &status_.num_entries_by_type,
      &status_.num_to_delete_entries_by_type);
}

void SyncBackupManager::SaveChanges() {
  NormalizeEntries();
  GetUserShare()->directory->SaveChanges();
}

SyncStatus SyncBackupManager::GetDetailedStatus() const {
  return status_;
}

ModelTypeSet SyncBackupManager::HandleTransactionEndingChangeEvent(
    const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  ModelTypeSet types;
  if (in_normalization_) {
    // Skip if in our own WriteTransaction from NormalizeEntries().
    in_normalization_ = false;
    return types;
  }

  for (syncable::EntryKernelMutationMap::const_iterator it =
      write_transaction_info.Get().mutations.Get().begin();
      it != write_transaction_info.Get().mutations.Get().end(); ++it) {
    int64 id = it->first;
    if (unsynced_.find(id) == unsynced_.end()) {
      unsynced_.insert(id);

      const syncable::EntryKernel& e = it->second.mutated;
      ModelType type = e.GetModelType();
      types.Put(type);
      if (!e.ref(syncable::ID).ServerKnows())
        status_.num_entries_by_type[type]++;
      if (e.ref(syncable::IS_DEL))
        status_.num_to_delete_entries_by_type[type]++;
    }
  }
  return types;
}

void SyncBackupManager::NormalizeEntries() {
  WriteTransaction trans(FROM_HERE, GetUserShare());
  in_normalization_ = true;
  for (std::set<int64>::const_iterator it = unsynced_.begin();
      it != unsynced_.end(); ++it) {
    syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                                 syncable::GET_BY_HANDLE, *it);
    CHECK(entry.good());

    if (!entry.GetId().ServerKnows())
      entry.PutId(syncable::Id::CreateFromServerId(entry.GetId().value()));
    entry.PutBaseVersion(1);
    entry.PutIsUnsynced(false);
  }
  unsynced_.clear();
}

}  // namespace syncer
