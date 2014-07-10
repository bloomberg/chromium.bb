// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager_base.h"

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/directory_backing_store.h"
#include "sync/syncable/mutable_entry.h"

namespace {

// Permanent bookmark folders as defined in bookmark_model_associator.cc.
// No mobile bookmarks because they only exists with sync enabled.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksTag[] = "other_bookmarks";

class DummyEntryptionHandler : public syncer::SyncEncryptionHandler {
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual void Init() OVERRIDE {}
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       bool is_explicit) OVERRIDE {}
  virtual void SetDecryptionPassphrase(const std::string& passphrase)
      OVERRIDE {}
  virtual void EnableEncryptEverything() OVERRIDE {}
  virtual bool EncryptEverythingEnabled() const OVERRIDE {
    return false;
  }
  virtual syncer::PassphraseType GetPassphraseType() const OVERRIDE {
    return syncer::KEYSTORE_PASSPHRASE;
  }
};

}  // anonymous namespace

namespace syncer {

SyncRollbackManagerBase::SyncRollbackManagerBase()
    : report_unrecoverable_error_function_(NULL),
      weak_ptr_factory_(this),
      dummy_handler_(new DummyEntryptionHandler),
      initialized_(false) {
}

SyncRollbackManagerBase::~SyncRollbackManagerBase() {
}

void SyncRollbackManagerBase::Init(
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
  unrecoverable_error_handler_ = unrecoverable_error_handler.Pass();
  report_unrecoverable_error_function_ = report_unrecoverable_error_function;

  if (!InitBackupDB(database_location, internal_components_factory)) {
    NotifyInitializationFailure();
    return;
  }

  initialized_ = true;
  NotifyInitializationSuccess();
}

ModelTypeSet SyncRollbackManagerBase::InitialSyncEndedTypes() {
  return share_.directory->InitialSyncEndedTypes();
}

ModelTypeSet SyncRollbackManagerBase::GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) {
  ModelTypeSet inited_types = share_.directory->InitialSyncEndedTypes();
  types.RemoveAll(inited_types);
  return types;
}

bool SyncRollbackManagerBase::PurgePartiallySyncedTypes() {
  NOTREACHED();
  return true;
}

void SyncRollbackManagerBase::UpdateCredentials(
    const SyncCredentials& credentials) {
}

void SyncRollbackManagerBase::StartSyncingNormally(
    const ModelSafeRoutingInfo& routing_info){
}

void SyncRollbackManagerBase::ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet to_download,
      ModelTypeSet to_purge,
      ModelTypeSet to_journal,
      ModelTypeSet to_unapply,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) {
  for (ModelTypeSet::Iterator type = to_download.First();
      type.Good(); type.Inc()) {
    if (InitTypeRootNode(type.Get())) {
      if (type.Get() == BOOKMARKS) {
        InitBookmarkFolder(kBookmarkBarTag);
        InitBookmarkFolder(kOtherBookmarksTag);
      }
    }
  }

  ready_task.Run();
}

void SyncRollbackManagerBase::SetInvalidatorEnabled(bool invalidator_enabled) {
}

void SyncRollbackManagerBase::OnIncomingInvalidation(
    syncer::ModelType type,
    scoped_ptr<InvalidationInterface> invalidation) {
  NOTREACHED();
}

void SyncRollbackManagerBase::AddObserver(SyncManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncRollbackManagerBase::RemoveObserver(SyncManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncStatus SyncRollbackManagerBase::GetDetailedStatus() const {
  return SyncStatus();
}

void SyncRollbackManagerBase::SaveChanges() {
}

void SyncRollbackManagerBase::ShutdownOnSyncThread() {
  if (initialized_) {
    SaveChanges();
    share_.directory->Close();
    share_.directory.reset();
    initialized_ = false;
  }
}

UserShare* SyncRollbackManagerBase::GetUserShare() {
  return &share_;
}

const std::string SyncRollbackManagerBase::cache_guid() {
  return share_.directory->cache_guid();
}

bool SyncRollbackManagerBase::ReceivedExperiment(Experiments* experiments) {
  return false;
}

bool SyncRollbackManagerBase::HasUnsyncedItems() {
  ReadTransaction trans(FROM_HERE, &share_);
  syncable::Directory::Metahandles unsynced;
  share_.directory->GetUnsyncedMetaHandles(trans.GetWrappedTrans(), &unsynced);
  return !unsynced.empty();
}

SyncEncryptionHandler* SyncRollbackManagerBase::GetEncryptionHandler() {
  return dummy_handler_.get();
}

void SyncRollbackManagerBase::RefreshTypes(ModelTypeSet types) {

}

void SyncRollbackManagerBase::HandleTransactionCompleteChangeEvent(
    ModelTypeSet models_with_changes) {
}

ModelTypeSet SyncRollbackManagerBase::HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) {
  return ModelTypeSet();
}

void SyncRollbackManagerBase::HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) {
}

void SyncRollbackManagerBase::HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) {
}

void SyncRollbackManagerBase::OnTransactionWrite(
    const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
    ModelTypeSet models_with_changes) {
}

void SyncRollbackManagerBase::NotifyInitializationSuccess() {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnInitializationComplete(
          MakeWeakHandle(base::WeakPtr<JsBackend>()),
          MakeWeakHandle(base::WeakPtr<DataTypeDebugInfoListener>()),
          true, InitialSyncEndedTypes()));
}

void SyncRollbackManagerBase::NotifyInitializationFailure() {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnInitializationComplete(
          MakeWeakHandle(base::WeakPtr<JsBackend>()),
          MakeWeakHandle(base::WeakPtr<DataTypeDebugInfoListener>()),
          false, ModelTypeSet()));
}

syncer::SyncContextProxy* SyncRollbackManagerBase::GetSyncContextProxy() {
  return NULL;
}

ScopedVector<syncer::ProtocolEvent>
SyncRollbackManagerBase::GetBufferedProtocolEvents() {
  return ScopedVector<syncer::ProtocolEvent>().Pass();
}

scoped_ptr<base::ListValue> SyncRollbackManagerBase::GetAllNodesForType(
    syncer::ModelType type) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  scoped_ptr<base::ListValue> nodes(
      trans.GetDirectory()->GetNodeDetailsForType(trans.GetWrappedTrans(),
                                                  type));
  return nodes.Pass();
}

bool SyncRollbackManagerBase::InitBackupDB(
    const base::FilePath& sync_folder,
    InternalComponentsFactory* internal_components_factory) {
  base::FilePath backup_db_path = sync_folder.Append(
      syncable::Directory::kSyncDatabaseFilename);
  scoped_ptr<syncable::DirectoryBackingStore> backing_store =
      internal_components_factory->BuildDirectoryBackingStore(
          "backup", backup_db_path).Pass();

  DCHECK(backing_store.get());
  share_.directory.reset(
      new syncable::Directory(
          backing_store.release(),
          unrecoverable_error_handler_.get(),
          report_unrecoverable_error_function_,
          NULL,
          NULL));
  return syncable::OPENED ==
      share_.directory->Open(
          "backup", this,
          MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));
}

bool SyncRollbackManagerBase::InitTypeRootNode(ModelType type) {
  WriteTransaction trans(FROM_HERE, &share_);
  ReadNode root(&trans);
  if (BaseNode::INIT_OK == root.InitTypeRoot(type))
    return true;

  syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                               syncable::CREATE_NEW_UPDATE_ITEM,
                               syncable::Id::CreateFromServerId(
                                   ModelTypeToString(type)));
  if (!entry.good())
    return false;

  entry.PutParentId(syncable::Id());
  entry.PutBaseVersion(1);
  entry.PutUniqueServerTag(ModelTypeToRootTag(type));
  entry.PutNonUniqueName(ModelTypeToString(type));
  entry.PutIsDel(false);
  entry.PutIsDir(true);

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(type, &specifics);
  entry.PutSpecifics(specifics);

  return true;
}

void SyncRollbackManagerBase::InitBookmarkFolder(const std::string& folder) {
  WriteTransaction trans(FROM_HERE, &share_);
  syncable::Entry bookmark_root(trans.GetWrappedTrans(),
                                syncable::GET_TYPE_ROOT,
                                BOOKMARKS);
  if (!bookmark_root.good())
    return;

  syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                               syncable::CREATE_NEW_UPDATE_ITEM,
                               syncable::Id::CreateFromServerId(folder));
  if (!entry.good())
    return;

  entry.PutParentId(bookmark_root.GetId());
  entry.PutBaseVersion(1);
  entry.PutUniqueServerTag(folder);
  entry.PutNonUniqueName(folder);
  entry.PutIsDel(false);
  entry.PutIsDir(true);

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(BOOKMARKS, &specifics);
  entry.PutSpecifics(specifics);
}

ObserverList<SyncManager::Observer>* SyncRollbackManagerBase::GetObservers() {
  return &observers_;
}

void SyncRollbackManagerBase::RegisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

void SyncRollbackManagerBase::UnregisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

bool SyncRollbackManagerBase::HasDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) { return false; }

void SyncRollbackManagerBase::RequestEmitDebugInfo() {}

}  // namespace syncer
