// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_backup_manager.h"

#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "url/gurl.h"

namespace syncer {

SyncBackupManager::SyncBackupManager()
    : in_normalization_(false) {
}

SyncBackupManager::~SyncBackupManager() {
}

void SyncBackupManager::Init(
    const base::FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& service_url,
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
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function,
    CancelationSignal* cancelation_signal) {
  if (SyncRollbackManagerBase::InitInternal(
          database_location,
          internal_components_factory,
          unrecoverable_error_handler.Pass(),
          report_unrecoverable_error_function)) {
    GetUserShare()->directory->CollectMetaHandleCounts(
        &status_.num_entries_by_type, &status_.num_to_delete_entries_by_type);

    HideSyncPreference(PRIORITY_PREFERENCES);
    HideSyncPreference(PREFERENCES);
  }
}

void SyncBackupManager::SaveChanges() {
  if (initialized()) {
    NormalizeEntries();
    GetUserShare()->directory->SaveChanges();
  }
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
    if (!entry.GetParentId().ServerKnows()) {
      entry.PutParentIdPropertyOnly(syncable::Id::CreateFromServerId(
          entry.GetParentId().value()));
    }
    entry.PutBaseVersion(1);
    entry.PutIsUnsynced(false);
  }
  unsynced_.clear();
}

void SyncBackupManager::HideSyncPreference(ModelType type) {
  WriteTransaction trans(FROM_HERE, GetUserShare());
  ReadNode pref_root(&trans);
  if (BaseNode::INIT_OK != pref_root.InitTypeRoot(type))
    return;

  std::vector<int64> pref_ids;
  pref_root.GetChildIds(&pref_ids);
  for (uint32 i = 0; i < pref_ids.size(); ++i) {
    syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                                 syncable::GET_BY_HANDLE, pref_ids[i]);
    if (entry.good()) {
      // HACKY: Set IS_DEL to true to remove entry from parent-children
      // index so that it's not returned when syncable service asks
      // for sync data. Syncable service then creates entry for local
      // model. Then the existing entry is undeleted and set to local value
      // because it has the same unique client tag.
      entry.PutIsDel(true);
      entry.PutIsUnsynced(false);

      // Don't persist on disk so that if backup is aborted before receiving
      // local preference values, values in sync DB are saved.
      GetUserShare()->directory->UnmarkDirtyEntry(
          trans.GetWrappedWriteTrans(), &entry);
    }
  }
}

void SyncBackupManager::RegisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

void SyncBackupManager::UnregisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

bool SyncBackupManager::HasDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) { return false; }

void SyncBackupManager::RequestEmitDebugInfo() {}

}  // namespace syncer
