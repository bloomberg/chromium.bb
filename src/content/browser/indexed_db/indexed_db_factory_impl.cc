// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory_impl.h"

#include <stdint.h>

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::ASCIIToUTF16;
using url::Origin;

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
      blink::kWebIDBDatabaseExceptionUnknownError,
      ASCIIToUTF16("Internal error opening backing store"
                   " for indexedDB.open."));
}

// Creates the leveldb and blob storage directories for IndexedDB.
std::tuple<base::FilePath /*leveldb_path*/,
           base::FilePath /*blob_path*/,
           leveldb::Status>
CreateDatabaseDirectories(const base::FilePath& path_base,
                          const url::Origin& origin) {
  leveldb::Status status;
  if (!base::CreateDirectoryAndGetError(path_base, nullptr)) {
    status =
        leveldb::Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status.ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                     origin);
    return {base::FilePath(), base::FilePath(), status};
  }

  base::FilePath leveldb_path =
      path_base.Append(indexed_db::GetLevelDBFileName(origin));
  base::FilePath blob_path =
      path_base.Append(indexed_db::GetBlobStoreFileName(origin));
  if (indexed_db::IsPathTooLong(leveldb_path)) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                     origin);
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
    indexed_db::LevelDBFactory* leveldb_factory,
    IndexedDBClassFactory* indexed_db_class_factory,
    base::Clock* clock)
    : context_(context),
      leveldb_factory_(leveldb_factory),
      indexed_db_class_factory_(indexed_db_class_factory),
      clock_(clock) {
  DCHECK(context);
  DCHECK(leveldb_factory);
  DCHECK(indexed_db_class_factory);
  DCHECK(clock);
}

IndexedDBFactoryImpl::~IndexedDBFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBFactoryImpl::GetDatabaseInfo(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseInfo");
  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(origin_state_handle, s, error, std::ignore, std::ignore) =
      GetOrOpenOriginFactory(origin, data_directory);
  if (!origin_state_handle.IsHeld() || !origin_state_handle.origin_state()) {
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  IndexedDBOriginState* factory = origin_state_handle.origin_state();

  IndexedDBMetadataCoding metadata_coding;
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  s = metadata_coding.ReadDatabaseNamesAndVersions(
      factory->backing_store_->db(),
      factory->backing_store_->origin_identifier(), &names_and_versions);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.databases().");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  callbacks->OnSuccess(std::move(names_and_versions));
}

void IndexedDBFactoryImpl::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseInfo");
  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(origin_state_handle, s, error, std::ignore, std::ignore) =
      GetOrOpenOriginFactory(origin, data_directory);
  if (!origin_state_handle.IsHeld() || !origin_state_handle.origin_state()) {
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  IndexedDBOriginState* factory = origin_state_handle.origin_state();

  IndexedDBMetadataCoding metadata_coding;
  std::vector<base::string16> names;
  s = metadata_coding.ReadDatabaseNames(
      factory->backing_store_->db(),
      factory->backing_store_->origin_identifier(), &names);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.webkitGetDatabaseNames.");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  callbacks->OnSuccess(names);
}

void IndexedDBFactoryImpl::Open(
    const base::string16& name,
    std::unique_ptr<IndexedDBPendingConnection> connection,
    const Origin& origin,
    const base::FilePath& data_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::Open");
  IndexedDBDatabase::Identifier unique_identifier(origin, name);
  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  std::tie(origin_state_handle, s, error, connection->data_loss_info,
           connection->was_cold_open) =
      GetOrOpenOriginFactory(origin, data_directory);
  if (!origin_state_handle.IsHeld() || !origin_state_handle.origin_state()) {
    connection->callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  IndexedDBOriginState* factory = origin_state_handle.origin_state();
  auto it = factory->databases().find(name);
  if (it != factory->databases().end()) {
    it->second->ScheduleOpenConnection(std::move(origin_state_handle),
                                       std::move(connection));
    return;
  }
  std::unique_ptr<IndexedDBDatabase> database;
  std::tie(database, s) = indexed_db_class_factory_->CreateIndexedDBDatabase(
      name, factory->backing_store(), this,
      base::BindRepeating(&IndexedDBFactoryImpl::OnDatabaseError,
                          weak_factory_.GetWeakPtr(), origin),
      factory->CreateDatabaseDeleteClosure(name),
      std::make_unique<IndexedDBMetadataCoding>(), std::move(unique_identifier),
      factory->lock_manager());
  if (!database.get()) {
    error = IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionUnknownError,
                                   ASCIIToUTF16("Internal error creating "
                                                "database backend for "
                                                "indexedDB.open."));
    connection->callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }

  // The database must be added before the schedule call, as the
  // CreateDatabaseDeleteClosure can be called synchronously.
  auto* database_ptr = database.get();
  factory->AddDatabase(name, std::move(database));
  database_ptr->ScheduleOpenConnection(std::move(origin_state_handle),
                                       std::move(connection));
}

void IndexedDBFactoryImpl::DeleteDatabase(
    const base::string16& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin, name);
  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(origin_state_handle, s, error, std::ignore, std::ignore) =
      GetOrOpenOriginFactory(origin, data_directory);
  if (!origin_state_handle.IsHeld() || !origin_state_handle.origin_state()) {
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  IndexedDBOriginState* factory = origin_state_handle.origin_state();

  auto it = factory->databases().find(name);
  if (it != factory->databases().end()) {
    base::WeakPtr<IndexedDBDatabase> database = it->second->AsWeakPtr();
    database->ScheduleDeleteDatabase(
        std::move(origin_state_handle), callbacks,
        base::BindOnce(&IndexedDBFactoryImpl::OnDatabaseDeleted,
                       weak_factory_.GetWeakPtr(), origin));
    if (force_close && database)
      database->ForceClose();
    return;
  }

  // TODO(dmurph): Get rid of on-demand metadata loading, and store metadata
  // in-memory in the backing store.
  IndexedDBMetadataCoding metadata_coding;
  std::vector<base::string16> names;
  s = metadata_coding.ReadDatabaseNames(
      factory->backing_store()->db(),
      factory->backing_store()->origin_identifier(), &names);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.deleteDatabase.");
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }

  if (!base::Contains(names, name)) {
    const int64_t version = 0;
    callbacks->OnSuccess(version);
    return;
  }

  std::unique_ptr<IndexedDBDatabase> database;
  std::tie(database, s) = indexed_db_class_factory_->CreateIndexedDBDatabase(
      name, factory->backing_store(), this,
      base::BindRepeating(&IndexedDBFactoryImpl::OnDatabaseError,
                          weak_factory_.GetWeakPtr(), origin),
      factory->CreateDatabaseDeleteClosure(name),
      std::make_unique<IndexedDBMetadataCoding>(), unique_identifier,
      factory->lock_manager());
  if (!database.get()) {
    error = IndexedDBDatabaseError(
        blink::kWebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error creating database backend for "
                     "indexedDB.deleteDatabase."));
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }

  base::WeakPtr<IndexedDBDatabase> database_ptr =
      factory->AddDatabase(name, std::move(database))->AsWeakPtr();
  database_ptr->ScheduleDeleteDatabase(
      std::move(origin_state_handle), std::move(callbacks),
      base::BindOnce(&IndexedDBFactoryImpl::OnDatabaseDeleted,
                     weak_factory_.GetWeakPtr(), origin));
  if (force_close && database_ptr)
    database_ptr->ForceClose();
}

void IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase");
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end()) {
    std::move(callback).Run(leveldb::Status::OK());
    return;
  }
  it->second->AbortAllTransactions(true);
  std::move(callback).Run(leveldb::Status::OK());
}

void IndexedDBFactoryImpl::AbortTransactionsForDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsForDatabase");
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end()) {
    std::move(callback).Run(leveldb::Status::OK());
    return;
  }
  it->second->AbortAllTransactions(false);
  std::move(callback).Run(leveldb::Status::OK());
}

void IndexedDBFactoryImpl::HandleBackingStoreFailure(const Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->ForceClose(origin,
                       IndexedDBContextImpl::FORCE_CLOSE_BACKING_STORE_FAILURE);
}

void IndexedDBFactoryImpl::HandleBackingStoreCorruption(
    const Origin& origin,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make a copy of origin as this is likely a reference to a member of a
  // backing store which this function will be deleting.
  Origin saved_origin(origin);
  DCHECK(context_);
  base::FilePath path_base = context_->data_path();

  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = base::UTF16ToUTF8(error.message());
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     path_base.AsUTF8Unsafe(), "...");
  IndexedDBBackingStore::RecordCorruptionInfo(path_base, saved_origin,
                                              sanitized_message);
  HandleBackingStoreFailure(saved_origin);
  // Note: DestroyBackingStore only deletes LevelDB files, leaving all others,
  //       so our corruption info file will remain.
  const base::FilePath file_path =
      path_base.Append(indexed_db::GetLevelDBFileName(saved_origin));
  leveldb::Status s = leveldb_factory_->DestroyLevelDB(file_path);
  DLOG_IF(ERROR, !s.ok()) << "Unable to delete backing store: " << s.ToString();
  UMA_HISTOGRAM_ENUMERATION(
      "WebCore.IndexedDB.DestroyCorruptBackingStoreStatus",
      leveldb_env::GetLevelDBStatusUMAValue(s),
      leveldb_env::LEVELDB_STATUS_MAX);
}

std::vector<IndexedDBDatabase*> IndexedDBFactoryImpl::GetOpenDatabasesForOrigin(
    const Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end()) {
    return std::vector<IndexedDBDatabase*>();
  }
  IndexedDBOriginState* factory = it->second.get();
  std::vector<IndexedDBDatabase*> out;
  out.reserve(factory->databases().size());
  std::for_each(factory->databases().begin(), factory->databases().end(),
                [&out](const auto& p) { out.push_back(p.second.get()); });
  return out;
}

void IndexedDBFactoryImpl::ForceClose(const Origin& origin,
                                      bool delete_in_memory_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return;

  IndexedDBOriginStateHandle origin_state_handle = it->second->CreateHandle();

  if (delete_in_memory_store)
    origin_state_handle.origin_state()->StopPersistingForIncognito();
  origin_state_handle.origin_state()->ForceClose();
}

void IndexedDBFactoryImpl::ForceSchemaDowngrade(const Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return;

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  leveldb::Status s = backing_store->RevertSchemaToV2();
  DLOG_IF(ERROR, !s.ok()) << "Unable to force downgrade: " << s.ToString();
}

V2SchemaCorruptionStatus IndexedDBFactoryImpl::HasV2SchemaCorruption(
    const Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return V2SchemaCorruptionStatus::kUnknown;

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  return backing_store->HasV2SchemaCorruption();
}

void IndexedDBFactoryImpl::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set |context_| to nullptr first to ensure no re-entry into the |cotext_|
  // object during shutdown. This can happen in methods like BlobFilesCleaned.
  context_ = nullptr;
  // Invalidate the weak factory that is used by the IndexedDBOriginStates to
  // destruct themselves. This prevents modification of the
  // |factories_per_origin_| map while it is iterated below, and allows us to
  // avoid holding a handle to call ForceClose();
  origin_state_destruction_weak_factory_.InvalidateWeakPtrs();
  for (const auto& pair : factories_per_origin_) {
    pair.second->ForceClose();
  }
  factories_per_origin_.clear();
}

void IndexedDBFactoryImpl::ReportOutstandingBlobs(const Origin& origin,
                                                  bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_)
    return;
  auto it = factories_per_origin_.find(origin);
  DCHECK(it != factories_per_origin_.end());

  it->second->ReportOutstandingBlobs(blobs_outstanding);
}

void IndexedDBFactoryImpl::BlobFilesCleaned(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->BlobFilesCleaned(origin);
}

size_t IndexedDBFactoryImpl::GetConnectionCount(const Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return 0;
  size_t count = 0;
  for (const auto& name_database_pair : it->second->databases()) {
    count += name_database_pair.second->ConnectionCount();
  }

  return count;
}

void IndexedDBFactoryImpl::NotifyIndexedDBContentChanged(
    const url::Origin& origin,
    const base::string16& database_name,
    const base::string16& object_store_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_)
    return;
  context_->NotifyIndexedDBContentChanged(origin, database_name,
                                          object_store_name);
}

int64_t IndexedDBFactoryImpl::GetInMemoryDBSize(const Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
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
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return base::Time();
  IndexedDBBackingStore* backing_store = it->second->backing_store();
  return backing_store->db()->LastModified();
}

std::vector<url::Origin> IndexedDBFactoryImpl::GetOpenOrigins() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<url::Origin> output;
  for (const auto& pair : factories_per_origin_) {
    output.push_back(pair.first);
  }
  return output;
}

IndexedDBOriginState* IndexedDBFactoryImpl::GetOriginFactory(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it != factories_per_origin_.end())
    return it->second.get();
  return nullptr;
}

std::tuple<IndexedDBOriginStateHandle,
           leveldb::Status,
           IndexedDBDatabaseError,
           IndexedDBDataLossInfo,
           /*is_cold_open=*/bool>
IndexedDBFactoryImpl::GetOrOpenOriginFactory(
    const Origin& origin,
    const base::FilePath& data_directory) {
  IDB_TRACE("indexed_db::GetOrOpenOriginFactory");
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are in sync.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it != factories_per_origin_.end()) {
    return {it->second->CreateHandle(), leveldb::Status::OK(),
            IndexedDBDatabaseError(), IndexedDBDataLossInfo(),
            /*was_cold_open=*/false};
  }

  bool is_incognito_and_in_memory = data_directory.empty();
  base::FilePath blob_path;
  base::FilePath database_path;
  leveldb::Status s = leveldb::Status::OK();
  if (!is_incognito_and_in_memory) {
    // The database will be on-disk and not in-memory.
    std::tie(database_path, blob_path, s) =
        CreateDatabaseDirectories(data_directory, origin);
    if (!s.ok())
      return {IndexedDBOriginStateHandle(), s, CreateDefaultError(),
              IndexedDBDataLossInfo(), /*was_cold_open=*/true};
  }
  IndexedDBDataLossInfo data_loss_info;
  std::unique_ptr<IndexedDBBackingStore> backing_store;
  bool disk_full = false;
  for (int i = 0; i < kNumOpenTries; ++i) {
    std::tie(backing_store, s, data_loss_info, disk_full) =
        OpenAndVerifyIndexedDBBackingStore(origin, data_directory,
                                           database_path, blob_path,
                                           /*is_first_attempt=*/i == 0);
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
      LOG(ERROR) << "Got corruption for " << origin.Serialize() << ", "
                 << sanitized_message;
      IndexedDBBackingStore::RecordCorruptionInfo(data_directory, origin,
                                                  sanitized_message);
    }
  }

  if (UNLIKELY(!s.ok())) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     origin);
    if (disk_full) {
      return {IndexedDBOriginStateHandle(), s,
              IndexedDBDatabaseError(
                  blink::kWebIDBDatabaseExceptionQuotaError,
                  ASCIIToUTF16("Encountered full disk while opening "
                               "backing store for indexedDB.open.")),
              data_loss_info, /*was_cold_open=*/true};

    } else {
      return {IndexedDBOriginStateHandle(), s, CreateDefaultError(),
              data_loss_info, /*was_cold_open=*/true};
    }
  }
  if (!is_incognito_and_in_memory)
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS, origin);
  it = factories_per_origin_
           .emplace(origin,
                    std::make_unique<IndexedDBOriginState>(
                        /*persist_for_incognito=*/is_incognito_and_in_memory,
                        clock_, leveldb_factory_, &earliest_sweep_,
                        base::BindOnce(
                            &IndexedDBFactoryImpl::RemoveOriginState,
                            origin_state_destruction_weak_factory_.GetWeakPtr(),
                            origin),
                        std::move(backing_store)))
           .first;
  context_->FactoryOpened(origin);
  return {it->second->CreateHandle(), s, IndexedDBDatabaseError(),
          data_loss_info, /*was_cold_open=*/true};
}

std::unique_ptr<IndexedDBBackingStore> IndexedDBFactoryImpl::CreateBackingStore(
    IndexedDBBackingStore::Mode backing_store_mode,
    indexed_db::LevelDBFactory* leveldb_factory,
    const url::Origin& origin,
    const base::FilePath& blob_path,
    std::unique_ptr<TransactionalLevelDBDatabase> db,
    base::SequencedTaskRunner* task_runner) {
  return std::make_unique<IndexedDBBackingStore>(
      backing_store_mode, this, leveldb_factory, origin, blob_path,
      std::move(db), task_runner);
}

std::tuple<std::unique_ptr<IndexedDBBackingStore>,
           leveldb::Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
IndexedDBFactoryImpl::OpenAndVerifyIndexedDBBackingStore(
    const url::Origin& origin,
    base::FilePath data_directory,
    base::FilePath database_path,
    base::FilePath blob_path,
    bool is_first_attempt) {
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
    std::string corruption_message =
        indexed_db::ReadCorruptionInfo(data_directory, origin);
    if (UNLIKELY(!corruption_message.empty())) {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      if (is_first_attempt) {
        ReportOpenStatus(
            indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
            origin);
      }
      data_loss_info.status = blink::mojom::IDBDataLoss::Total;
      data_loss_info.message = base::StrCat(
          {"IndexedDB (database was corrupt): ", corruption_message});
      // This is a special case where we want to make sure the database is
      // deleted, so we try to delete again.
      status = leveldb_factory_->DestroyLevelDB(database_path);
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.IndexedDB.DestroyCorruptBackingStoreStatus",
          leveldb_env::GetLevelDBStatusUMAValue(status),
          leveldb_env::LEVELDB_STATUS_MAX);
      if (UNLIKELY(!status.ok())) {
        LOG(ERROR) << "Unable to delete backing store: " << status.ToString();
        return {nullptr, status, data_loss_info, /*is_disk_full=*/false};
      }
    }
  }

  bool is_disk_full;
  scoped_refptr<LevelDBState> state;

  // Open the leveldb database.
  std::tie(state, status, is_disk_full) = leveldb_factory_->OpenLevelDBState(
      database_path, indexed_db::GetDefaultIndexedDBComparator(),
      indexed_db::GetDefaultLevelDBComparator());

  if (UNLIKELY(!status.ok()))
    return {nullptr, status, IndexedDBDataLossInfo(), is_disk_full};

  std::unique_ptr<TransactionalLevelDBDatabase> database =
      std::make_unique<TransactionalLevelDBDatabase>(
          std::move(state), leveldb_factory_, context_->TaskRunner(),
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
        origin);
    return {nullptr, status, std::move(data_loss_info), /*is_disk_full=*/false};
  } else if (UNLIKELY(!are_schemas_known)) {
    LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it as "
                  "failure to open.";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
        origin);
    return {nullptr, leveldb::Status::Corruption("Unknown IndexedDB schema"),
            std::move(data_loss_info), /*is_disk_full=*/false};
  }

  bool first_open_since_startup =
      backends_opened_since_startup_.insert(origin).second;
  IndexedDBBackingStore::Mode backing_store_mode =
      is_incognito_and_in_memory ? IndexedDBBackingStore::Mode::kInMemory
                                 : IndexedDBBackingStore::Mode::kOnDisk;
  std::unique_ptr<IndexedDBBackingStore> backing_store = CreateBackingStore(
      backing_store_mode, leveldb_factory_, origin, blob_path,
      std::move(database), context_->TaskRunner());
  status = backing_store->Initialize(
      /*cleanup_live_journal=*/(!is_incognito_and_in_memory &&
                                first_open_since_startup));

  if (UNLIKELY(!status.ok()))
    return {nullptr, status, IndexedDBDataLossInfo(), /*is_disk_full=*/false};

  return {std::move(backing_store), status, std::move(data_loss_info),
          /*is_disk_full=*/false};
}

void IndexedDBFactoryImpl::RemoveOriginState(const url::Origin& origin) {
  factories_per_origin_.erase(origin);
}

void IndexedDBFactoryImpl::OnDatabaseError(const url::Origin& origin,
                                           leveldb::Status status,
                                           const char* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  if (status.IsCorruption()) {
    IndexedDBDatabaseError error =
        message != nullptr
            ? IndexedDBDatabaseError(
                  blink::kWebIDBDatabaseExceptionUnknownError, message)
            : IndexedDBDatabaseError(
                  blink::kWebIDBDatabaseExceptionUnknownError,
                  base::ASCIIToUTF16(status.ToString()));
    HandleBackingStoreCorruption(origin, error);
  } else {
    HandleBackingStoreFailure(origin);
  }
}

void IndexedDBFactoryImpl::OnDatabaseDeleted(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_)
    return;
  context_->DatabaseDeleted(origin);
}

bool IndexedDBFactoryImpl::IsDatabaseOpen(const Origin& origin,
                                          const base::string16& name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return false;
  return base::Contains(it->second->databases(), name);
}

bool IndexedDBFactoryImpl::IsBackingStoreOpen(const Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(factories_per_origin_, origin);
}

bool IndexedDBFactoryImpl::IsBackingStorePendingClose(
    const Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_per_origin_.find(origin);
  if (it == factories_per_origin_.end())
    return false;
  return it->second->IsClosing();
}

}  // namespace content
