// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory_impl.h"

#include <inttypes.h>
#include <stdint.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {
constexpr static const int kNumOpenTries = 2;

leveldb::Status GetDBSizeFromEnv(leveldb::Env* env,
                                 const std::string& path,
                                 int64_t* total_size_out) {
  *total_size_out = 0;
  // Root path should be /, but in MemEnv, a path name is not tailed with '/'
  DCHECK_EQ(path.back(), '/');
  const std::string path_without_slash = path.substr(0, path.length() - 1);

  // This assumes that leveldb will not put a subdirectory into the directory
  std::vector<std::string> file_names;
  leveldb::Status s = env->GetChildren(path_without_slash, &file_names);
  if (!s.ok())
    return s;

  for (std::string& file_name : file_names) {
    file_name.insert(0, path);
    uint64_t file_size;
    s = env->GetFileSize(file_name, &file_size);
    if (!s.ok())
      return s;
    else
      *total_size_out += static_cast<int64_t>(file_size);
  }
  return s;
}

IndexedDBDatabaseError CreateDefaultError() {
  return IndexedDBDatabaseError(
      blink::mojom::IDBException::kUnknownError,
      u"Internal error opening backing store for indexedDB.open.");
}

// Creates the leveldb and blob storage directories for IndexedDB.
std::tuple<base::FilePath /*leveldb_path*/,
           base::FilePath /*blob_path*/,
           leveldb::Status>
CreateDatabaseDirectories(storage::FilesystemProxy* filesystem,
                          const base::FilePath& path_base,
                          const blink::StorageKey& storage_key) {
  leveldb::Status status;
  if (filesystem->CreateDirectory(path_base) != base::File::Error::FILE_OK) {
    status =
        leveldb::Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status.ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                     storage_key);
    return {base::FilePath(), base::FilePath(), status};
  }

  base::FilePath leveldb_path =
      path_base.Append(indexed_db::GetLevelDBFileName(storage_key));
  base::FilePath blob_path =
      path_base.Append(indexed_db::GetBlobStoreFileName(storage_key));
  if (indexed_db::IsPathTooLong(filesystem, leveldb_path)) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                     storage_key);
    status = leveldb::Status::IOError("File path too long");
    return {base::FilePath(), base::FilePath(), status};
  }
  return {leveldb_path, blob_path, status};
}

std::tuple<bool, leveldb::Status> AreSchemasKnown(
    TransactionalLevelDBDatabase* db) {
  int64_t db_schema_version = 0;
  bool found = false;
  leveldb::Status s = indexed_db::GetInt(db, SchemaVersionKey::Encode(),
                                         &db_schema_version, &found);
  if (!s.ok())
    return {false, s};
  if (!found) {
    return {true, s};
  }
  if (db_schema_version < 0)
    return {false, leveldb::Status::Corruption(
                       "Invalid IndexedDB database schema version.")};
  if (db_schema_version > indexed_db::kLatestKnownSchemaVersion) {
    return {false, s};
  }

  int64_t raw_db_data_version = 0;
  s = indexed_db::GetInt(db, DataVersionKey::Encode(), &raw_db_data_version,
                         &found);
  if (!s.ok())
    return {false, s};
  if (!found) {
    return {true, s};
  }
  if (raw_db_data_version < 0)
    return {false,
            leveldb::Status::Corruption("Invalid IndexedDB data version.")};

  return {IndexedDBDataFormatVersion::GetCurrent().IsAtLeast(
              IndexedDBDataFormatVersion::Decode(raw_db_data_version)),
          s};
}

}  // namespace

IndexedDBFactoryImpl::IndexedDBFactoryImpl(
    IndexedDBContextImpl* context,
    IndexedDBClassFactory* indexed_db_class_factory,
    base::Clock* clock)
    : context_(context),
      class_factory_(indexed_db_class_factory),
      clock_(clock) {
  DCHECK(context);
  DCHECK(indexed_db_class_factory);
  DCHECK(clock);
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "IndexedDBFactoryImpl", base::SequencedTaskRunnerHandle::Get(),
          base::trace_event::MemoryDumpProvider::Options());
}

IndexedDBFactoryImpl::~IndexedDBFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void IndexedDBFactoryImpl::GetDatabaseInfo(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const blink::StorageKey& storage_key,
    const base::FilePath& data_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseInfo");
  IndexedDBStorageKeyStateHandle storage_key_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(storage_key_state_handle, s, error, std::ignore, std::ignore) =
      GetOrOpenStorageKeyFactory(storage_key, data_directory,
                                 /*create_if_missing=*/false);
  if (!storage_key_state_handle.IsHeld() ||
      !storage_key_state_handle.storage_key_state()) {
    if (s.IsNotFound()) {
      callbacks->OnSuccess(std::move(names_and_versions));
    } else {
      callbacks->OnError(error);
    }
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }
  IndexedDBStorageKeyState* factory =
      storage_key_state_handle.storage_key_state();

  IndexedDBMetadataCoding metadata_coding;
  s = metadata_coding.ReadDatabaseNamesAndVersions(
      factory->backing_store_->db(),
      factory->backing_store_->origin_identifier(), &names_and_versions);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.databases().");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }
  callbacks->OnSuccess(std::move(names_and_versions));
}

void IndexedDBFactoryImpl::Open(
    const std::u16string& name,
    std::unique_ptr<IndexedDBPendingConnection> connection,
    const blink::StorageKey& storage_key,
    const base::FilePath& data_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::Open");
  IndexedDBDatabase::Identifier unique_identifier(storage_key, name);
  IndexedDBStorageKeyStateHandle storage_key_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  std::tie(storage_key_state_handle, s, error, connection->data_loss_info,
           connection->was_cold_open) =
      GetOrOpenStorageKeyFactory(storage_key, data_directory,
                                 /*create_if_missing=*/true);
  if (!storage_key_state_handle.IsHeld() ||
      !storage_key_state_handle.storage_key_state()) {
    connection->callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }
  IndexedDBStorageKeyState* factory =
      storage_key_state_handle.storage_key_state();
  auto it = factory->databases().find(name);
  if (it != factory->databases().end()) {
    it->second->ScheduleOpenConnection(std::move(storage_key_state_handle),
                                       std::move(connection));
    return;
  }
  std::unique_ptr<IndexedDBDatabase> database;
  std::tie(database, s) = class_factory_->CreateIndexedDBDatabase(
      name, factory->backing_store(), this,
      base::BindRepeating(
          &IndexedDBFactoryImpl::MaybeRunTasksForStorageKey,
          storage_key_state_destruction_weak_factory_.GetWeakPtr(),
          storage_key),
      std::make_unique<IndexedDBMetadataCoding>(), std::move(unique_identifier),
      factory->lock_manager());
  if (!database.get()) {
    error = IndexedDBDatabaseError(
        blink::mojom::IDBException::kUnknownError,
        u"Internal error creating database backend for indexedDB.open.");
    connection->callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }

  // The database must be added before the schedule call, as the
  // CreateDatabaseDeleteClosure can be called synchronously.
  auto* database_ptr = database.get();
  factory->AddDatabase(name, std::move(database));
  database_ptr->ScheduleOpenConnection(std::move(storage_key_state_handle),
                                       std::move(connection));
}

void IndexedDBFactoryImpl::DeleteDatabase(
    const std::u16string& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const blink::StorageKey& storage_key,
    const base::FilePath& data_directory,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(storage_key, name);
  IndexedDBStorageKeyStateHandle storage_key_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(storage_key_state_handle, s, error, std::ignore, std::ignore) =
      GetOrOpenStorageKeyFactory(storage_key, data_directory,
                                 /*create_if_missing=*/true);
  if (!storage_key_state_handle.IsHeld() ||
      !storage_key_state_handle.storage_key_state()) {
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }
  IndexedDBStorageKeyState* factory =
      storage_key_state_handle.storage_key_state();

  auto it = factory->databases().find(name);
  if (it != factory->databases().end()) {
    base::WeakPtr<IndexedDBDatabase> database = it->second->AsWeakPtr();
    database->ScheduleDeleteDatabase(
        std::move(storage_key_state_handle), callbacks,
        base::BindOnce(&IndexedDBFactoryImpl::OnDatabaseDeleted,
                       weak_factory_.GetWeakPtr(), storage_key));
    if (force_close) {
      leveldb::Status status = database->ForceCloseAndRunTasks();
      if (!status.ok())
        OnDatabaseError(storage_key, status, "Error aborting transactions.");
    }
    return;
  }

  // TODO(dmurph): Get rid of on-demand metadata loading, and store metadata
  // in-memory in the backing store.
  IndexedDBMetadataCoding metadata_coding;
  std::vector<std::u16string> names;
  s = metadata_coding.ReadDatabaseNames(
      factory->backing_store()->db(),
      factory->backing_store()->origin_identifier(), &names);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.deleteDatabase.");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }

  if (!base::Contains(names, name)) {
    const int64_t version = 0;
    callbacks->OnSuccess(version);
    return;
  }

  std::unique_ptr<IndexedDBDatabase> database;
  std::tie(database, s) = class_factory_->CreateIndexedDBDatabase(
      name, factory->backing_store(), this,
      base::BindRepeating(
          &IndexedDBFactoryImpl::MaybeRunTasksForStorageKey,
          storage_key_state_destruction_weak_factory_.GetWeakPtr(),
          storage_key),
      std::make_unique<IndexedDBMetadataCoding>(), unique_identifier,
      factory->lock_manager());
  if (!database.get()) {
    error = IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                   u"Internal error creating database backend "
                                   u"for indexedDB.deleteDatabase.");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(storage_key, error);
    return;
  }

  base::WeakPtr<IndexedDBDatabase> database_ptr =
      factory->AddDatabase(name, std::move(database))->AsWeakPtr();
  database_ptr->ScheduleDeleteDatabase(
      std::move(storage_key_state_handle), std::move(callbacks),
      base::BindOnce(&IndexedDBFactoryImpl::OnDatabaseDeleted,
                     weak_factory_.GetWeakPtr(), storage_key));
  if (force_close) {
    leveldb::Status status = database_ptr->ForceCloseAndRunTasks();
    if (!status.ok())
      OnDatabaseError(storage_key, status, "Error aborting transactions.");
  }
}

void IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase");
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end()) {
    std::move(callback).Run(leveldb::Status::OK());
    return;
  }
  it->second->AbortAllTransactions(true);
  RunTasksForStorageKey(it->second->AsWeakPtr());
  std::move(callback).Run(leveldb::Status::OK());
}

void IndexedDBFactoryImpl::AbortTransactionsForDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsForDatabase");
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end()) {
    std::move(callback).Run(leveldb::Status::OK());
    return;
  }
  it->second->AbortAllTransactions(false);
  RunTasksForStorageKey(it->second->AsWeakPtr());
  std::move(callback).Run(leveldb::Status::OK());
}

void IndexedDBFactoryImpl::HandleBackingStoreFailure(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // nullptr after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->ForceCloseSync(
      storage_key,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_BACKING_STORE_FAILURE);
}

void IndexedDBFactoryImpl::HandleBackingStoreCorruption(
    const blink::StorageKey& storage_key,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make a copy of storage_key as this is likely a reference to a member of a
  // backing store which this function will be deleting.
  blink::StorageKey saved_storage_key(storage_key);
  DCHECK(context_);
  base::FilePath path_base = context_->data_path();

  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = base::UTF16ToUTF8(error.message());
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     path_base.AsUTF8Unsafe(), "...");
  IndexedDBBackingStore::RecordCorruptionInfo(path_base, saved_storage_key,
                                              sanitized_message);
  HandleBackingStoreFailure(saved_storage_key);
  // Note: DestroyBackingStore only deletes LevelDB files, leaving all others,
  //       so our corruption info file will remain.
  //       The blob directory will be deleted when the database is recreated
  //       the next time it is opened.
  const base::FilePath file_path =
      path_base.Append(indexed_db::GetLevelDBFileName(saved_storage_key));
  leveldb::Status s =
      class_factory_->leveldb_factory().DestroyLevelDB(file_path);
  DLOG_IF(ERROR, !s.ok()) << "Unable to delete backing store: " << s.ToString();
  base::UmaHistogramEnumeration(
      "WebCore.IndexedDB.DestroyCorruptBackingStoreStatus",
      leveldb_env::GetLevelDBStatusUMAValue(s),
      leveldb_env::LEVELDB_STATUS_MAX);
}

std::vector<IndexedDBDatabase*>
IndexedDBFactoryImpl::GetOpenDatabasesForStorageKey(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end()) {
    return std::vector<IndexedDBDatabase*>();
  }
  IndexedDBStorageKeyState* factory = it->second.get();
  std::vector<IndexedDBDatabase*> out;
  out.reserve(factory->databases().size());
  std::for_each(factory->databases().begin(), factory->databases().end(),
                [&out](const auto& p) { out.push_back(p.second.get()); });
  return out;
}

void IndexedDBFactoryImpl::ForceClose(const blink::StorageKey& storage_key,
                                      bool delete_in_memory_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return;

  base::WeakPtr<IndexedDBStorageKeyState> weak_ptr;
  {
    IndexedDBStorageKeyStateHandle storage_key_state_handle =
        it->second->CreateHandle();

    if (delete_in_memory_store)
      storage_key_state_handle.storage_key_state()
          ->StopPersistingForIncognito();
    storage_key_state_handle.storage_key_state()->ForceClose();
    weak_ptr = storage_key_state_handle.storage_key_state()->AsWeakPtr();
  }
  // Run tasks so the storage_key state is deleted.
  RunTasksForStorageKey(std::move(weak_ptr));
}

void IndexedDBFactoryImpl::ForceSchemaDowngrade(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return;

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  leveldb::Status s = backing_store->RevertSchemaToV2();
  DLOG_IF(ERROR, !s.ok()) << "Unable to force downgrade: " << s.ToString();
}

V2SchemaCorruptionStatus IndexedDBFactoryImpl::HasV2SchemaCorruption(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return V2SchemaCorruptionStatus::kUnknown;

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  return backing_store->HasV2SchemaCorruption();
}

void IndexedDBFactoryImpl::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set |context_| to nullptr first to ensure no re-entry into the |cotext_|
  // object during shutdown. This can happen in methods like BlobFilesCleaned.
  context_ = nullptr;
  // Invalidate the weak factory that is used by the IndexedDBStorageKeyStates
  // to destruct themselves. This prevents modification of the
  // |factories_per_storage_key_| map while it is iterated below, and allows us
  // to avoid holding a handle to call ForceClose();
  storage_key_state_destruction_weak_factory_.InvalidateWeakPtrs();
  for (const auto& pair : factories_per_storage_key_) {
    pair.second->ForceClose();
  }
  factories_per_storage_key_.clear();
}

void IndexedDBFactoryImpl::ReportOutstandingBlobs(
    const blink::StorageKey& storage_key,
    bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_)
    return;
  auto it = factories_per_storage_key_.find(storage_key);
  DCHECK(it != factories_per_storage_key_.end());

  it->second->ReportOutstandingBlobs(blobs_outstanding);
}

void IndexedDBFactoryImpl::BlobFilesCleaned(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // nullptr after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->BlobFilesCleaned(storage_key);
}

size_t IndexedDBFactoryImpl::GetConnectionCount(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return 0;
  size_t count = 0;
  for (const auto& name_database_pair : it->second->databases()) {
    count += name_database_pair.second->ConnectionCount();
  }

  return count;
}

void IndexedDBFactoryImpl::NotifyIndexedDBContentChanged(
    const blink::StorageKey& storage_key,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_)
    return;
  context_->NotifyIndexedDBContentChanged(storage_key, database_name,
                                          object_store_name);
}

int64_t IndexedDBFactoryImpl::GetInMemoryDBSize(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return 0;
  IndexedDBBackingStore* backing_store = it->second->backing_store();
  int64_t level_db_size = 0;
  leveldb::Status s =
      GetDBSizeFromEnv(backing_store->db()->env(), "/", &level_db_size);
  if (!s.ok())
    LOG(ERROR) << "Failed to GetDBSizeFromEnv: " << s.ToString();

  return backing_store->GetInMemoryBlobSize() + level_db_size;
}

base::Time IndexedDBFactoryImpl::GetLastModified(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return base::Time();
  IndexedDBBackingStore* backing_store = it->second->backing_store();
  return backing_store->db()->LastModified();
}

std::vector<blink::StorageKey> IndexedDBFactoryImpl::GetOpenStorageKeys()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<blink::StorageKey> output;
  output.reserve(factories_per_storage_key_.size());
  for (const auto& pair : factories_per_storage_key_) {
    output.push_back(pair.first);
  }
  return output;
}

IndexedDBStorageKeyState* IndexedDBFactoryImpl::GetStorageKeyFactory(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it != factories_per_storage_key_.end())
    return it->second.get();
  return nullptr;
}

std::tuple<IndexedDBStorageKeyStateHandle,
           leveldb::Status,
           IndexedDBDatabaseError,
           IndexedDBDataLossInfo,
           /*is_cold_open=*/bool>
IndexedDBFactoryImpl::GetOrOpenStorageKeyFactory(
    const blink::StorageKey& storage_key,
    const base::FilePath& data_directory,
    bool create_if_missing) {
  IDB_TRACE("indexed_db::GetOrOpenStorageKeyFactory");
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are in sync.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it != factories_per_storage_key_.end()) {
    return {it->second->CreateHandle(), leveldb::Status::OK(),
            IndexedDBDatabaseError(), IndexedDBDataLossInfo(),
            /*was_cold_open=*/false};
  }
  UMA_HISTOGRAM_ENUMERATION(
      indexed_db::kBackingStoreActionUmaName,
      indexed_db::IndexedDBAction::kBackingStoreOpenAttempt);

  bool is_incognito_and_in_memory = data_directory.empty();
  base::FilePath blob_path;
  base::FilePath database_path;
  leveldb::Status s = leveldb::Status::OK();
  if (!is_incognito_and_in_memory) {
    // The database will be on-disk and not in-memory.
    auto filesystem_proxy = storage::CreateFilesystemProxy();
    std::tie(database_path, blob_path, s) = CreateDatabaseDirectories(
        filesystem_proxy.get(), data_directory, storage_key);
    if (!s.ok())
      return {IndexedDBStorageKeyStateHandle(), s, CreateDefaultError(),
              IndexedDBDataLossInfo(), /*was_cold_open=*/true};
  }

  // TODO(dmurph) Have these factories be given in the constructor, or as
  // arguments to this method.
  DefaultLevelDBScopesFactory scopes_factory;
  std::unique_ptr<DisjointRangeLockManager> lock_manager =
      std::make_unique<DisjointRangeLockManager>(kIndexedDBLockLevelCount);
  IndexedDBDataLossInfo data_loss_info;
  std::unique_ptr<IndexedDBBackingStore> backing_store;
  bool disk_full = false;
  base::ElapsedTimer open_timer;
  leveldb::Status first_try_status;
  for (int i = 0; i < kNumOpenTries; ++i) {
    LevelDBScopesOptions scopes_options;
    scopes_options.lock_manager = lock_manager.get();
    scopes_options.metadata_key_prefix = ScopesPrefix::Encode();
    scopes_options.failure_callback = base::BindRepeating(
        [](const blink::StorageKey& storage_key,
           base::WeakPtr<IndexedDBFactoryImpl> factory, leveldb::Status s) {
          if (!factory)
            return;
          factory->OnDatabaseError(storage_key, s, nullptr);
        },
        storage_key, weak_factory_.GetWeakPtr());
    const bool is_first_attempt = i == 0;
    auto filesystem_proxy = !is_incognito_and_in_memory
                                ? storage::CreateFilesystemProxy()
                                : nullptr;
    std::tie(backing_store, s, data_loss_info, disk_full) =
        OpenAndVerifyIndexedDBBackingStore(
            storage_key, data_directory, database_path, blob_path,
            std::move(scopes_options), &scopes_factory,
            std::move(filesystem_proxy), is_first_attempt, create_if_missing);
    if (LIKELY(is_first_attempt))
      first_try_status = s;
    if (LIKELY(s.ok()))
      break;
    DCHECK(!backing_store);
    // If the disk is full, always exit immediately.
    if (disk_full)
      break;
    if (s.IsCorruption()) {
      std::string sanitized_message = leveldb_env::GetCorruptionMessage(s);
      base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                         data_directory.AsUTF8Unsafe(), "...");
      LOG(ERROR) << "Got corruption for " << storage_key.GetDebugString()
                 << ", " << sanitized_message;
      IndexedDBBackingStore::RecordCorruptionInfo(data_directory, storage_key,
                                                  sanitized_message);
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      "WebCore.IndexedDB.BackingStore.OpenFirstTryResult",
      leveldb_env::GetLevelDBStatusUMAValue(first_try_status),
      leveldb_env::LEVELDB_STATUS_MAX);

  if (LIKELY(first_try_status.ok())) {
    UMA_HISTOGRAM_TIMES(
        "WebCore.IndexedDB.BackingStore.OpenFirstTrySuccessTime",
        open_timer.Elapsed());
  }

  if (UNLIKELY(!s.ok())) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     storage_key);

    if (disk_full) {
      context_->quota_manager_proxy()->NotifyWriteFailed(storage_key);
      return {IndexedDBStorageKeyStateHandle(), s,
              IndexedDBDatabaseError(blink::mojom::IDBException::kQuotaError,
                                     u"Encountered full disk while opening "
                                     "backing store for indexedDB.open."),
              data_loss_info, /*was_cold_open=*/true};

    } else {
      return {IndexedDBStorageKeyStateHandle(), s, CreateDefaultError(),
              data_loss_info, /*was_cold_open=*/true};
    }
  }
  DCHECK(backing_store);
  // Scopes must be single sequence to keep methods like ForceClose synchronous.
  // See https://crbug.com/980685
  s = backing_store->db()->scopes()->StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  if (UNLIKELY(!s.ok())) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     storage_key);

    return {IndexedDBStorageKeyStateHandle(), s, CreateDefaultError(),
            data_loss_info, /*was_cold_open=*/true};
  }

  if (!is_incognito_and_in_memory)
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
                     storage_key);

  auto run_tasks_callback = base::BindRepeating(
      &IndexedDBFactoryImpl::MaybeRunTasksForStorageKey,
      storage_key_state_destruction_weak_factory_.GetWeakPtr(), storage_key);

  auto tear_down_callback = base::BindRepeating(
      [](const blink::StorageKey& storage_key,
         base::WeakPtr<IndexedDBFactoryImpl> factory, leveldb::Status s) {
        if (!factory)
          return;
        factory->OnDatabaseError(storage_key, s, nullptr);
      },
      storage_key, weak_factory_.GetWeakPtr());

  auto storage_key_state = std::make_unique<IndexedDBStorageKeyState>(
      storage_key,
      /*persist_for_incognito=*/is_incognito_and_in_memory, clock_,
      &class_factory_->transactional_leveldb_factory(), &earliest_sweep_,
      &earliest_compaction_, std::move(lock_manager),
      std::move(run_tasks_callback), std::move(tear_down_callback),
      std::move(backing_store));

  it = factories_per_storage_key_
           .emplace(storage_key, std::move(storage_key_state))
           .first;
  context_->FactoryOpened(storage_key);
  return {it->second->CreateHandle(), s, IndexedDBDatabaseError(),
          data_loss_info, /*was_cold_open=*/true};
}

std::unique_ptr<IndexedDBBackingStore> IndexedDBFactoryImpl::CreateBackingStore(
    IndexedDBBackingStore::Mode backing_store_mode,
    TransactionalLevelDBFactory* transactional_leveldb_factory,
    const blink::StorageKey& storage_key,
    const base::FilePath& blob_path,
    std::unique_ptr<TransactionalLevelDBDatabase> db,
    storage::mojom::BlobStorageContext* blob_storage_context,
    storage::mojom::FileSystemAccessContext* file_system_access_context,
    std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
    IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
    IndexedDBBackingStore::ReportOutstandingBlobsCallback
        report_outstanding_blobs,
    scoped_refptr<base::SequencedTaskRunner> idb_task_runner) {
  return std::make_unique<IndexedDBBackingStore>(
      backing_store_mode, transactional_leveldb_factory, storage_key, blob_path,
      std::move(db), blob_storage_context, file_system_access_context,
      std::move(filesystem_proxy), std::move(blob_files_cleaned),
      std::move(report_outstanding_blobs), std::move(idb_task_runner));
}
std::tuple<std::unique_ptr<IndexedDBBackingStore>,
           leveldb::Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
IndexedDBFactoryImpl::OpenAndVerifyIndexedDBBackingStore(
    const blink::StorageKey& storage_key,
    base::FilePath data_directory,
    base::FilePath database_path,
    base::FilePath blob_path,
    LevelDBScopesOptions scopes_options,
    LevelDBScopesFactory* scopes_factory,
    std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
    bool is_first_attempt,
    bool create_if_missing) {
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are in sync.
  DCHECK_EQ(database_path.empty(), data_directory.empty());
  DCHECK_EQ(blob_path.empty(), data_directory.empty());
  IDB_TRACE("indexed_db::OpenAndVerifyLevelDBDatabase");

  bool is_incognito_and_in_memory = data_directory.empty();
  leveldb::Status status;
  IndexedDBDataLossInfo data_loss_info;
  data_loss_info.status = blink::mojom::IDBDataLoss::None;
  if (!is_incognito_and_in_memory) {
    // Check for previous corruption, and if found then try to delete the
    // database.
    std::string corruption_message = indexed_db::ReadCorruptionInfo(
        filesystem_proxy.get(), data_directory, storage_key);
    if (UNLIKELY(!corruption_message.empty())) {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      if (is_first_attempt) {
        ReportOpenStatus(
            indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
            storage_key);
      }
      data_loss_info.status = blink::mojom::IDBDataLoss::Total;
      data_loss_info.message = base::StrCat(
          {"IndexedDB (database was corrupt): ", corruption_message});
      // This is a special case where we want to make sure the database is
      // deleted, so we try to delete again.
      status = class_factory_->leveldb_factory().DestroyLevelDB(database_path);
      base::UmaHistogramEnumeration(
          "WebCore.IndexedDB.DestroyCorruptBackingStoreStatus",
          leveldb_env::GetLevelDBStatusUMAValue(status),
          leveldb_env::LEVELDB_STATUS_MAX);
      if (UNLIKELY(!status.ok())) {
        LOG(ERROR) << "Unable to delete backing store: " << status.ToString();
        return {nullptr, status, data_loss_info, /*is_disk_full=*/false};
      }
    }
  }

  // Open the leveldb database.
  scoped_refptr<LevelDBState> database_state;
  bool is_disk_full;
  {
    IDB_TRACE("IndexedDBFactoryImpl::OpenLevelDB");
    base::TimeTicks begin_time = base::TimeTicks::Now();
    size_t write_buffer_size = leveldb_env::WriteBufferSize(
        base::SysInfo::AmountOfTotalDiskSpace(database_path));
    std::tie(database_state, status, is_disk_full) =
        class_factory_->leveldb_factory().OpenLevelDBState(
            database_path, create_if_missing, write_buffer_size);
    if (UNLIKELY(!status.ok())) {
      if (!status.IsNotFound()) {
        indexed_db::ReportLevelDBError("WebCore.IndexedDB.LevelDBOpenErrors",
                                       status);
      }
      return {nullptr, status, IndexedDBDataLossInfo(), is_disk_full};
    }
    UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.LevelDB.OpenTime",
                               base::TimeTicks::Now() - begin_time);
  }

  // Create the LevelDBScopes wrapper.
  std::unique_ptr<LevelDBScopes> scopes;
  {
    IDB_TRACE("IndexedDBFactoryImpl::OpenLevelDBScopes");
    DCHECK(scopes_factory);
    std::tie(scopes, status) = scopes_factory->CreateAndInitializeLevelDBScopes(
        std::move(scopes_options), database_state);
    if (UNLIKELY(!status.ok()))
      return {nullptr, status, std::move(data_loss_info),
              /*is_disk_full=*/false};
  }

  // Create the TransactionalLevelDBDatabase wrapper.
  std::unique_ptr<TransactionalLevelDBDatabase> database =
      class_factory_->transactional_leveldb_factory().CreateLevelDBDatabase(
          std::move(database_state), std::move(scopes),
          context_->IDBTaskRunner(),
          TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);

  bool are_schemas_known = false;
  std::tie(are_schemas_known, status) = AreSchemasKnown(database.get());
  if (UNLIKELY(!status.ok())) {
    LOG(ERROR) << "IndexedDB had an error checking schema, treating it as "
                  "failure to open: "
               << status.ToString();
    ReportOpenStatus(
        indexed_db::
            INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
        storage_key);
    return {nullptr, status, std::move(data_loss_info), /*is_disk_full=*/false};
  } else if (UNLIKELY(!are_schemas_known)) {
    LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it as "
                  "failure to open.";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
        storage_key);
    return {nullptr, leveldb::Status::Corruption("Unknown IndexedDB schema"),
            std::move(data_loss_info), /*is_disk_full=*/false};
  }

  bool first_open_since_startup =
      backends_opened_since_startup_.insert(storage_key).second;
  IndexedDBBackingStore::Mode backing_store_mode =
      is_incognito_and_in_memory ? IndexedDBBackingStore::Mode::kInMemory
                                 : IndexedDBBackingStore::Mode::kOnDisk;
  std::unique_ptr<IndexedDBBackingStore> backing_store = CreateBackingStore(
      backing_store_mode, &class_factory_->transactional_leveldb_factory(),
      storage_key, blob_path, std::move(database),
      context_->blob_storage_context(), context_->file_system_access_context(),
      std::move(filesystem_proxy),
      base::BindRepeating(&IndexedDBFactoryImpl::BlobFilesCleaned,
                          weak_factory_.GetWeakPtr(), storage_key),
      base::BindRepeating(&IndexedDBFactoryImpl::ReportOutstandingBlobs,
                          weak_factory_.GetWeakPtr(), storage_key),
      context_->IDBTaskRunner());
  status = backing_store->Initialize(
      /*clean_active_blob_journal=*/(!is_incognito_and_in_memory &&
                                     first_open_since_startup));

  if (UNLIKELY(!status.ok()))
    return {nullptr, status, IndexedDBDataLossInfo(), /*is_disk_full=*/false};

  return {std::move(backing_store), status, std::move(data_loss_info),
          /*is_disk_full=*/false};
}

void IndexedDBFactoryImpl::RemoveStorageKeyState(
    const blink::StorageKey& storage_key) {
  factories_per_storage_key_.erase(storage_key);
}

void IndexedDBFactoryImpl::OnDatabaseError(const blink::StorageKey& storage_key,
                                           leveldb::Status status,
                                           const char* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  if (status.IsCorruption()) {
    IndexedDBDatabaseError error =
        message != nullptr
            ? IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                     message)
            : IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                     base::ASCIIToUTF16(status.ToString()));
    HandleBackingStoreCorruption(storage_key, error);
  } else {
    if (status.IsIOError()) {
      context_->quota_manager_proxy()->NotifyWriteFailed(storage_key);
    }
    HandleBackingStoreFailure(storage_key);
  }
}

void IndexedDBFactoryImpl::OnDatabaseDeleted(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (call_on_database_deleted_for_testing_)
    call_on_database_deleted_for_testing_.Run(storage_key);

  if (!context_)
    return;
  context_->DatabaseDeleted(storage_key);
}

void IndexedDBFactoryImpl::MaybeRunTasksForStorageKey(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return;

  IndexedDBStorageKeyState* storage_key_state = it->second.get();
  if (storage_key_state->is_task_run_scheduled())
    return;

  storage_key_state->set_task_run_scheduled();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBFactoryImpl::RunTasksForStorageKey,
                     storage_key_state_destruction_weak_factory_.GetWeakPtr(),
                     storage_key_state->AsWeakPtr()));
}

void IndexedDBFactoryImpl::RunTasksForStorageKey(
    base::WeakPtr<IndexedDBStorageKeyState> storage_key_state) {
  if (!storage_key_state)
    return;
  IndexedDBStorageKeyState::RunTasksResult result;
  leveldb::Status status;
  std::tie(result, status) = storage_key_state->RunTasks();
  switch (result) {
    case IndexedDBStorageKeyState::RunTasksResult::kDone:
      return;
    case IndexedDBStorageKeyState::RunTasksResult::kError:
      OnDatabaseError(storage_key_state->storage_key(), status, nullptr);
      return;
    case IndexedDBStorageKeyState::RunTasksResult::kCanBeDestroyed:
      factories_per_storage_key_.erase(storage_key_state->storage_key());
      return;
  }
}

bool IndexedDBFactoryImpl::IsDatabaseOpen(const blink::StorageKey& storage_key,
                                          const std::u16string& name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return false;
  return base::Contains(it->second->databases(), name);
}

bool IndexedDBFactoryImpl::IsBackingStoreOpen(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(factories_per_storage_key_, storage_key);
}

bool IndexedDBFactoryImpl::IsBackingStorePendingClose(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_storage_key_.find(storage_key);
  if (it == factories_per_storage_key_.end())
    return false;
  return it->second->IsClosing();
}

bool IndexedDBFactoryImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& storage_key_state_pair : factories_per_storage_key_) {
    IndexedDBStorageKeyState* state = storage_key_state_pair.second.get();
    base::CheckedNumeric<uint64_t> total_memory_in_flight = 0;
    for (const auto& db_name_object_pair : state->databases()) {
      for (IndexedDBConnection* connection :
           db_name_object_pair.second->connections()) {
        for (const auto& txn_id_pair : connection->transactions()) {
          total_memory_in_flight += txn_id_pair.second->in_flight_memory();
        }
      }
    }
    // This pointer is used to match the pointer used in
    // TransactionalLevelDBDatabase::OnMemoryDump.
    leveldb::DB* db = state->backing_store()->db()->db();
    auto* db_dump = pmd->CreateAllocatorDump(
        base::StringPrintf("site_storage/index_db/in_flight_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(db)));
    db_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       total_memory_in_flight.ValueOrDefault(0));
  }
  return true;
}

void IndexedDBFactoryImpl::CallOnDatabaseDeletedForTesting(
    OnDatabaseDeletedCallback callback) {
  call_on_database_deleted_for_testing_ = std::move(callback);
}

}  // namespace content
