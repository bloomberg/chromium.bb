// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/values.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_bucket_state.h"
#include "content/browser/indexed_db/indexed_db_bucket_state_handle.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/origin.h"

using base::DictionaryValue;
using base::ListValue;
using storage::DatabaseUtil;

namespace content {

namespace {

static MockBrowserTestIndexedDBClassFactory* GetTestClassFactory() {
  static ::base::LazyInstance<MockBrowserTestIndexedDBClassFactory>::Leaky
      s_factory = LAZY_INSTANCE_INITIALIZER;
  return s_factory.Pointer();
}

static IndexedDBClassFactory* GetTestIDBClassFactory() {
  return GetTestClassFactory();
}

bool IsAllowedPath(const std::vector<base::FilePath>& allowed_paths,
                   const base::FilePath& candidate_path) {
  for (const base::FilePath& allowed_path : allowed_paths) {
    if (candidate_path == allowed_path || allowed_path.IsParent(candidate_path))
      return true;
  }
  return false;
}

// This may be called after the IndexedDBContext is destroyed.
const std::map<blink::StorageKey, base::FilePath>
DefaultBucketFilePerFirstPartyStorageKey(
    const base::FilePath& first_party_path) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB sequence,
  // if a global handle to it is ever available.
  std::map<blink::StorageKey, base::FilePath> storage_key_to_file_path;
  if (first_party_path.empty())
    return storage_key_to_file_path;
  base::FileEnumerator file_enumerator(first_party_path, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == indexed_db::kLevelDBExtension &&
        file_path.RemoveExtension().Extension() ==
            indexed_db::kIndexedDBExtension) {
      std::string storage_key_id = file_path.BaseName()
                                       .RemoveExtension()
                                       .RemoveExtension()
                                       .MaybeAsASCII();
      storage_key_to_file_path[blink::StorageKey(
          storage::GetOriginFromIdentifier(storage_key_id))] = file_path;
    }
  }
  return storage_key_to_file_path;
}

// This may be called after the IndexedDBContext is destroyed.
const std::map<storage::BucketId, base::FilePath>
DefaultBucketFilePerThirdPartyBucketId(const base::FilePath& third_party_path) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB sequence,
  // if a global handle to it is ever available.
  std::map<storage::BucketId, base::FilePath> bucket_id_to_file_path;
  if (third_party_path.empty())
    return bucket_id_to_file_path;
  base::FileEnumerator file_enumerator(third_party_path, /*recursive=*/true,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.BaseName().Extension() == indexed_db::kLevelDBExtension &&
        file_path.BaseName().RemoveExtension().value() ==
            indexed_db::kIndexedDBFile &&
        file_path.DirName().BaseName().value() ==
            storage::kIndexedDbDirectory) {
      int64_t raw_bucket_id = 0;
      bool success = base::StringToInt64(
          file_path.DirName().DirName().BaseName().value(), &raw_bucket_id);
      if (success && raw_bucket_id > 0) {
        bucket_id_to_file_path[storage::BucketId::FromUnsafeValue(
            raw_bucket_id)] = file_path;
      }
    }
  }
  return bucket_id_to_file_path;
}

}  // namespace

// static
void IndexedDBContextImpl::ReleaseOnIDBSequence(
    scoped_refptr<IndexedDBContextImpl>&& context) {
  if (!context->IDBTaskRunner()->RunsTasksInCurrentSequence()) {
    IndexedDBContextImpl* context_ptr = context.get();
    context_ptr->IDBTaskRunner()->ReleaseSoon(FROM_HERE, std::move(context));
  }
}

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& base_data_path,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::Clock* clock,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner)
    : idb_task_runner_(
          custom_task_runner
              ? custom_task_runner
              : (base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::WithBaseSyncPrimitives(),
                     base::TaskPriority::USER_VISIBLE,
                     // BLOCK_SHUTDOWN to support clearing session-only storage.
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}))),
      dispatcher_host_(this, std::move(io_task_runner)),
      base_data_path_(base_data_path.empty() ? base::FilePath()
                                             : base_data_path),
      force_keep_session_state_(false),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      clock_(clock),
      quota_client_(std::make_unique<IndexedDBQuotaClient>(*this)),
      quota_client_wrapper_(
          std::make_unique<storage::QuotaClientCallbackWrapper>(
              quota_client_.get())),
      quota_client_receiver_(quota_client_wrapper_.get()),
      filesystem_proxy_(storage::CreateFilesystemProxy()) {
  TRACE_EVENT0("IndexedDB", "init");

  // QuotaManagerProxy::RegisterClient() must be called during construction
  // until crbug.com/1182630 is fixed.
  mojo::PendingRemote<storage::mojom::QuotaClient> quota_client_remote;
  mojo::PendingReceiver<storage::mojom::QuotaClient> quota_client_receiver =
      quota_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy_->RegisterClient(
      std::move(quota_client_remote),
      storage::QuotaClientType::kIndexedDatabase,
      {blink::mojom::StorageType::kTemporary});
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IndexedDBContextImpl::BindPipesOnIDBSequence,
                                weak_factory_.GetWeakPtr(),
                                std::move(quota_client_receiver),
                                std::move(blob_storage_context),
                                std::move(file_system_access_context)));
}

void IndexedDBContextImpl::BindPipesOnIDBSequence(
    mojo::PendingReceiver<storage::mojom::QuotaClient>
        pending_quota_client_receiver,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        pending_blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        pending_file_system_access_context) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (pending_quota_client_receiver) {
    quota_client_receiver_.Bind(std::move(pending_quota_client_receiver));
  }
  if (pending_blob_storage_context) {
    blob_storage_context_.Bind(std::move(pending_blob_storage_context));
  }
  if (pending_file_system_access_context) {
    file_system_access_context_.Bind(
        std::move(pending_file_system_access_context));
  }
}

void IndexedDBContextImpl::Bind(
    mojo::PendingReceiver<storage::mojom::IndexedDBControl> control) {
  // We cannot run this in the constructor it needs to be async, but the async
  // tasks might not finish before the destructor runs.
  InitializeFromFilesIfNeeded(base::DoNothing());
  receivers_.Add(this, std::move(control));
}

void IndexedDBContextImpl::BindIndexedDB(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  GetOrCreateDefaultBucket(
      storage_key,
      base::BindOnce(&IndexedDBContextImpl::BindIndexedDBImpl,
                     weak_factory_.GetWeakPtr(), std::move(receiver)));
}

void IndexedDBContextImpl::BindIndexedDBImpl(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
    const absl::optional<storage::BucketLocator>& bucket_locator) {
  dispatcher_host_.AddReceiver(bucket_locator, std::move(receiver));
}

void IndexedDBContextImpl::GetUsage(GetUsageCallback usage_callback) {
  InitializeFromFilesIfNeeded(
      base::BindOnce(&IndexedDBContextImpl::GetUsageImpl,
                     weak_factory_.GetWeakPtr(), std::move(usage_callback)));
}

void IndexedDBContextImpl::GetUsageImpl(GetUsageCallback usage_callback) {
  std::vector<storage::mojom::StorageUsageInfoPtr> result;
  for (const auto& bucket_locator : GetAllBuckets()) {
    // TODO(https://crbug.com/1199077): Pass the real StorageKey when
    // StorageUsageInfo is converted.
    result.emplace_back(storage::mojom::StorageUsageInfo::New(
        bucket_locator.storage_key.origin(), GetBucketDiskUsage(bucket_locator),
        GetBucketLastModified(bucket_locator)));
  }
  std::move(usage_callback).Run(std::move(result));
}

// Note - this is being kept async (instead of having a 'sync' version) to allow
// ForceClose to become asynchronous.  This is required for
// https://crbug.com/965142.
void IndexedDBContextImpl::DeleteForBucket(const blink::StorageKey& storage_key,
                                           DeleteForBucketCallback callback) {
  GetOrCreateDefaultBucket(
      storage_key,
      base::BindOnce(&IndexedDBContextImpl::DeleteForBucketImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::DeleteForBucket(
    const storage::BucketLocator& bucket_locator,
    DeleteForBucketCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
  bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  DeleteForBucketImpl(std::move(callback), bucket_locator);
}

void IndexedDBContextImpl::DeleteForBucketImpl(
    DeleteForBucketCallback callback,
    const absl::optional<storage::BucketLocator>& bucket_locator) {
  if (!bucket_locator) {
    std::move(callback).Run(true);
    return;
  }
  ForceClose(*bucket_locator,
             storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
             base::DoNothing());
  // InitializeFromFilesIfNeeded might not have finished, so we need to check
  // if there's a file in the directory and not exit early if so.
  const auto& storage_key_to_file_path =
      DefaultBucketFilePerFirstPartyStorageKey(GetFirstPartyDataPath());
  const auto& bucket_id_to_file_path =
      DefaultBucketFilePerThirdPartyBucketId(GetThirdPartyDataPath());
  if (!HasBucket(*bucket_locator) &&
      storage_key_to_file_path.find(bucket_locator->storage_key) ==
          storage_key_to_file_path.end() &&
      bucket_id_to_file_path.find(bucket_locator->id) ==
          bucket_id_to_file_path.end()) {
    std::move(callback).Run(true);
    return;
  }

  if (is_incognito()) {
    bucket_set_.erase(*bucket_locator);
    bucket_size_map_.erase(*bucket_locator);
    storage_key_to_bucket_locator_.erase(bucket_locator->storage_key);
    bucket_id_to_bucket_locator_.erase(bucket_locator->id);
    std::move(callback).Run(true);
    return;
  }

  base::FilePath idb_directory = GetLevelDBPath(*bucket_locator);
  EnsureDiskUsageCacheInitialized(*bucket_locator);

  leveldb::Status s =
      IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(
          idb_directory);
  bool success = s.ok();
  if (success)
    success = filesystem_proxy_->DeletePathRecursively(
        GetBlobStorePath(*bucket_locator));
  QueryDiskAndUpdateQuotaUsage(*bucket_locator);
  if (success) {
    bucket_set_.erase(*bucket_locator);
    bucket_size_map_.erase(*bucket_locator);
    storage_key_to_bucket_locator_.erase(bucket_locator->storage_key);
    bucket_id_to_bucket_locator_.erase(bucket_locator->id);
  }
  std::move(callback).Run(success);
}

void IndexedDBContextImpl::ForceClose(const blink::StorageKey& storage_key,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  GetOrCreateDefaultBucket(
      storage_key,
      base::BindOnce(&IndexedDBContextImpl::ForceCloseImpl,
                     weak_factory_.GetWeakPtr(), reason, std::move(closure)));
}

void IndexedDBContextImpl::ForceClose(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::ForceCloseReason reason,
    base::OnceClosure closure) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
  bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  ForceCloseImpl(reason, std::move(closure), bucket_locator);
}

void IndexedDBContextImpl::ForceCloseImpl(
    const storage::mojom::ForceCloseReason reason,
    base::OnceClosure closure,
    const absl::optional<storage::BucketLocator>& bucket_locator) {
  base::UmaHistogramEnumeration("WebCore.IndexedDB.Context.ForceCloseReason",
                                reason);
  if (!bucket_locator || !HasBucket(*bucket_locator)) {
    std::move(closure).Run();
    return;
  }

  if (!indexeddb_factory_.get()) {
    std::move(closure).Run();
    return;
  }

  // Make a copy of storage_key, as the ref might go away here during the close.
  auto bucket_locator_copy = *bucket_locator;
  indexeddb_factory_->ForceClose(
      bucket_locator_copy,
      reason == storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  DCHECK_EQ(0UL, GetConnectionCountSync(bucket_locator_copy));
  std::move(closure).Run();
}

void IndexedDBContextImpl::GetConnectionCount(
    const blink::StorageKey& storage_key,
    GetConnectionCountCallback callback) {
  GetOrCreateDefaultBucket(
      storage_key,
      base::BindOnce(&IndexedDBContextImpl::GetConnectionCountImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::GetConnectionCount(
    const storage::BucketLocator& bucket_locator,
    GetConnectionCountCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
  bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  GetConnectionCountImpl(std::move(callback), bucket_locator);
}

void IndexedDBContextImpl::GetConnectionCountImpl(
    GetConnectionCountCallback callback,
    const absl::optional<storage::BucketLocator>& bucket_locator) {
  size_t count = 0;
  if (bucket_locator) {
    count = GetConnectionCountSync(*bucket_locator);
  }
  std::move(callback).Run(count);
}

size_t IndexedDBContextImpl::GetConnectionCountSync(
    const storage::BucketLocator& bucket_locator) {
  size_t count = 0;
  if (HasBucket(bucket_locator) && indexeddb_factory_.get()) {
    count = indexeddb_factory_->GetConnectionCount(bucket_locator);
  }
  return count;
}

void IndexedDBContextImpl::DownloadBucketData(
    const blink::StorageKey& storage_key,
    DownloadBucketDataCallback callback) {
  GetOrCreateDefaultBucket(
      storage_key,
      base::BindOnce(&IndexedDBContextImpl::DownloadBucketDataImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::DownloadBucketData(
    const storage::BucketLocator& bucket_locator,
    DownloadBucketDataCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
  bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  return DownloadBucketDataImpl(std::move(callback), bucket_locator);
}

void IndexedDBContextImpl::DownloadBucketDataImpl(
    DownloadBucketDataCallback callback,
    const absl::optional<storage::BucketLocator>& bucket_locator) {
  bool success = false;

  // Make sure the database hasn't been deleted.
  if (!bucket_locator || !HasBucket(*bucket_locator)) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  ForceClose(*bucket_locator,
             storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
             base::DoNothing());

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  // This will need to get cleaned up after the download has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string storage_key_id =
      storage::GetIdentifierFromOrigin(bucket_locator->storage_key.origin());
  base::FilePath zip_path = temp_path.AppendASCII(storage_key_id)
                                .AddExtension(FILE_PATH_LITERAL("zip"));

  std::vector<base::FilePath> paths = GetStoragePaths(*bucket_locator);
  zip::ZipWithFilterCallback(GetDataPath(*bucket_locator), zip_path,
                             base::BindRepeating(IsAllowedPath, paths));

  success = true;
  std::move(callback).Run(success, temp_path, zip_path);
}

void IndexedDBContextImpl::GetAllBucketsDetails(
    GetAllBucketsDetailsCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<storage::BucketLocator> bucket_locators = GetAllBuckets();

  std::sort(bucket_locators.begin(), bucket_locators.end());

  base::Value::List list;
  for (const auto& bucket_locator : bucket_locators) {
    base::Value info(base::Value::Type::DICTIONARY);
    // TODO(https://crbug.com/1199077): Serialize storage key directly
    // once supported by OriginDetails.
    info.SetStringKey("url", bucket_locator.storage_key.origin().Serialize());
    info.SetDoubleKey("size",
                      static_cast<double>(GetBucketDiskUsage(bucket_locator)));
    info.SetDoubleKey("last_modified",
                      GetBucketLastModified(bucket_locator).ToJsTime());

    base::Value paths(base::Value::Type::LIST);
    if (!is_incognito()) {
      for (const base::FilePath& path : GetStoragePaths(bucket_locator))
        paths.Append(path.AsUTF8Unsafe());
    } else {
      paths.Append("N/A");
    }
    info.SetKey("paths", std::move(paths));
    info.SetDoubleKey("connection_count",
                      GetConnectionCountSync(bucket_locator));

    // This ends up being O(NlogN), where N = number of open databases. We
    // iterate over all open databases to extract just those in the
    // bucket_locator, and we're iterating over all bucket_locators in the outer
    // loop.

    if (!indexeddb_factory_.get()) {
      list.Append(std::move(info));
      continue;
    }
    std::vector<IndexedDBDatabase*> databases =
        indexeddb_factory_->GetOpenDatabasesForBucket(bucket_locator);
    // TODO(jsbell): Sort by name?
    base::Value database_list(base::Value::Type::LIST);

    for (IndexedDBDatabase* db : databases) {
      base::Value db_info(base::Value::Type::DICTIONARY);

      db_info.SetStringKey("name", db->name());
      db_info.SetDoubleKey("connection_count", db->ConnectionCount());
      db_info.SetDoubleKey("active_open_delete", db->ActiveOpenDeleteCount());
      db_info.SetDoubleKey("pending_open_delete", db->PendingOpenDeleteCount());

      base::Value transaction_list(base::Value::Type::LIST);

      for (IndexedDBConnection* connection : db->connections()) {
        for (const auto& transaction_id_pair : connection->transactions()) {
          const auto* transaction = transaction_id_pair.second.get();
          base::Value transaction_info(base::Value::Type::DICTIONARY);

          switch (transaction->mode()) {
            case blink::mojom::IDBTransactionMode::ReadOnly:
              transaction_info.SetStringKey("mode", "readonly");
              break;
            case blink::mojom::IDBTransactionMode::ReadWrite:
              transaction_info.SetStringKey("mode", "readwrite");
              break;
            case blink::mojom::IDBTransactionMode::VersionChange:
              transaction_info.SetStringKey("mode", "versionchange");
              break;
          }

          switch (transaction->state()) {
            case IndexedDBTransaction::CREATED:
              transaction_info.SetStringKey("status", "blocked");
              break;
            case IndexedDBTransaction::STARTED:
              if (transaction->diagnostics().tasks_scheduled > 0)
                transaction_info.SetStringKey("status", "running");
              else
                transaction_info.SetStringKey("status", "started");
              break;
            case IndexedDBTransaction::COMMITTING:
              transaction_info.SetStringKey("status", "committing");
              break;
            case IndexedDBTransaction::FINISHED:
              transaction_info.SetStringKey("status", "finished");
              break;
          }

          transaction_info.SetDoubleKey("tid", transaction->id());
          transaction_info.SetDoubleKey(
              "age",
              (base::Time::Now() - transaction->diagnostics().creation_time)
                  .InMillisecondsF());
          transaction_info.SetDoubleKey(
              "runtime",
              (base::Time::Now() - transaction->diagnostics().start_time)
                  .InMillisecondsF());
          transaction_info.SetDoubleKey(
              "tasks_scheduled", transaction->diagnostics().tasks_scheduled);
          transaction_info.SetDoubleKey(
              "tasks_completed", transaction->diagnostics().tasks_completed);

          base::Value scope(base::Value::Type::LIST);
          for (const auto& id : transaction->scope()) {
            auto stores_it = db->metadata().object_stores.find(id);
            if (stores_it != db->metadata().object_stores.end())
              scope.Append(stores_it->second.name);
          }

          transaction_info.SetKey("scope", std::move(scope));
          transaction_list.Append(std::move(transaction_info));
        }
      }
      db_info.SetKey("transactions", std::move(transaction_list));

      database_list.Append(std::move(db_info));
    }
    info.SetKey("databases", std::move(database_list));
    list.Append(std::move(info));
  }

  std::move(callback).Run(is_incognito(), std::move(list));
}

void IndexedDBContextImpl::SetForceKeepSessionState() {
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<IndexedDBContextImpl> context) {
                       if (context)
                         context->force_keep_session_state_ = true;
                     },
                     weak_factory_.GetWeakPtr()));
}

void IndexedDBContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  for (const auto& update : policy_updates) {
    if (!update->purge_on_shutdown) {
      sites_to_purge_on_shutdown_.erase(net::SchemefulSite(update->origin));
    } else {
      sites_to_purge_on_shutdown_.insert(net::SchemefulSite(update->origin));
    }
  }
}

void IndexedDBContextImpl::BindTestInterface(
    mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  test_receivers_.Add(this, std::move(receiver));
}

void IndexedDBContextImpl::AddObserver(
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBContextImpl> context,
             mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
            if (context)
              context->observers_.Add(std::move(observer));
          },
          weak_factory_.GetWeakPtr(), std::move(observer)));
}

void IndexedDBContextImpl::GetBaseDataPathForTesting(
    GetBaseDataPathForTestingCallback callback) {
  std::move(callback).Run(GetFirstPartyDataPath());
}

void IndexedDBContextImpl::GetFilePathForTesting(
    const storage::BucketLocator& bucket_locator,
    GetFilePathForTestingCallback callback) {
  std::move(callback).Run(GetLevelDBPath(bucket_locator));
}

void IndexedDBContextImpl::ResetCachesForTesting(base::OnceClosure callback) {
  bucket_set_.clear();
  bucket_size_map_.clear();
  std::move(callback).Run();
}

void IndexedDBContextImpl::ForceSchemaDowngradeForTesting(
    const storage::BucketLocator& bucket_locator,
    ForceSchemaDowngradeForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasBucket(bucket_locator)) {
    std::move(callback).Run(false);
    return;
  }

  if (indexeddb_factory_.get()) {
    indexeddb_factory_->ForceSchemaDowngrade(bucket_locator);
    std::move(callback).Run(true);
    return;
  }
  ForceClose(
      bucket_locator,
      storage::mojom::ForceCloseReason::FORCE_SCHEMA_DOWNGRADE_INTERNALS_PAGE,
      base::DoNothing());
  std::move(callback).Run(false);
}

void IndexedDBContextImpl::HasV2SchemaCorruptionForTesting(
    const storage::BucketLocator& bucket_locator,
    HasV2SchemaCorruptionForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasBucket(bucket_locator)) {
    std::move(callback).Run(
        storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
    return;
  }

  if (indexeddb_factory_.get()) {
    std::move(callback).Run(
        static_cast<storage::mojom::V2SchemaCorruptionStatus>(
            indexeddb_factory_->HasV2SchemaCorruption(bucket_locator)));
    return;
  }
  return std::move(callback).Run(
      storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
}

void IndexedDBContextImpl::WriteToIndexedDBForTesting(
    const storage::BucketLocator& bucket_locator,
    const std::string& key,
    const std::string& value,
    base::OnceClosure callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.bucket_state()->backing_store()->db();
  std::string value_copy = value;
  s = db->Put(key, &value_copy);
  CHECK(s.ok()) << s.ToString();
  handle.Release();
  GetIDBFactory()->ForceClose(bucket_locator, true);
  std::move(callback).Run();
}

void IndexedDBContextImpl::GetBlobCountForTesting(
    const storage::BucketLocator& bucket_locator,
    GetBlobCountForTestingCallback callback) {
  std::move(callback).Run(GetBucketBlobFileCount(bucket_locator));
}

void IndexedDBContextImpl::GetNextBlobNumberForTesting(
    const storage::BucketLocator& bucket_locator,
    int64_t database_id,
    GetNextBlobNumberForTestingCallback callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.bucket_state()->backing_store()->db();

  const std::string key_gen_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);
  std::string data;
  bool found = false;
  bool ok = db->Get(key_gen_key, &data, &found).ok();
  CHECK(found);
  CHECK(ok);
  base::StringPiece slice(data);
  int64_t number;
  CHECK(DecodeVarInt(&slice, &number));
  CHECK(DatabaseMetaDataKey::IsValidBlobNumber(number));

  std::move(callback).Run(number);
}

void IndexedDBContextImpl::GetPathForBlobForTesting(
    const storage::BucketLocator& bucket_locator,
    int64_t database_id,
    int64_t blob_number,
    GetPathForBlobForTestingCallback callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  IndexedDBBackingStore* backing_store = handle.bucket_state()->backing_store();
  base::FilePath path =
      backing_store->GetBlobFileName(database_id, blob_number);
  std::move(callback).Run(path);
}

void IndexedDBContextImpl::CompactBackingStoreForTesting(
    const storage::BucketLocator& bucket_locator,
    base::OnceClosure callback) {
  IndexedDBFactoryImpl* factory = GetIDBFactory();

  std::vector<IndexedDBDatabase*> databases =
      factory->GetOpenDatabasesForBucket(bucket_locator);

  if (!databases.empty()) {
    // Compact the first db's backing store since all the db's are in the same
    // backing store.
    IndexedDBDatabase* db = databases[0];
    IndexedDBBackingStore* backing_store = db->backing_store();
    backing_store->Compact();
  }
  std::move(callback).Run();
}

void IndexedDBContextImpl::BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver) {
  // Lazily instantiate the GetTestClassFactory.
  if (!mock_failure_injector_.has_value())
    mock_failure_injector_.emplace(GetTestClassFactory());

  // TODO(enne): this should really not be a static setter.
  CHECK(!mock_failure_injector_->is_bound());
  GetTestClassFactory()->Reset();
  IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetTestIDBClassFactory);

  mock_failure_injector_->Bind(std::move(receiver));
  mock_failure_injector_->set_disconnect_handler(base::BindOnce([]() {
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(nullptr);
  }));
}

void IndexedDBContextImpl::GetDatabaseKeysForTesting(
    GetDatabaseKeysForTestingCallback callback) {
  std::move(callback).Run(SchemaVersionKey::Encode(), DataVersionKey::Encode());
}

IndexedDBFactoryImpl* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!indexeddb_factory_.get()) {
    indexeddb_factory_ = std::make_unique<IndexedDBFactoryImpl>(
        this, IndexedDBClassFactory::Get(), clock_);
  }
  return indexeddb_factory_.get();
}

std::vector<storage::BucketLocator> IndexedDBContextImpl::GetAllBuckets() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  return std::vector<storage::BucketLocator>(bucket_set_.begin(),
                                             bucket_set_.end());
}

bool IndexedDBContextImpl::HasBucket(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  return bucket_set_.find(bucket_locator) != bucket_set_.end();
}

int IndexedDBContextImpl::GetBucketBlobFileCount(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  int count = 0;
  base::FileEnumerator file_enumerator(GetBlobStorePath(bucket_locator), true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

int64_t IndexedDBContextImpl::GetBucketDiskUsage(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasBucket(bucket_locator))
    return 0;
  EnsureDiskUsageCacheInitialized(bucket_locator);
  return bucket_size_map_[bucket_locator];
}

base::Time IndexedDBContextImpl::GetBucketLastModified(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasBucket(bucket_locator))
    return base::Time();

  if (is_incognito()) {
    if (!indexeddb_factory_)
      return base::Time();
    return indexeddb_factory_->GetLastModified(bucket_locator);
  }

  base::FilePath idb_directory = GetLevelDBPath(bucket_locator);
  absl::optional<base::File::Info> info =
      filesystem_proxy_->GetFileInfo(idb_directory);
  if (!info.has_value())
    return base::Time();
  return info->last_modified;
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const storage::BucketLocator& bucket_locator) const {
  std::vector<base::FilePath> paths = {GetLevelDBPath(bucket_locator),
                                       GetBlobStorePath(bucket_locator)};
  return paths;
}

const base::FilePath IndexedDBContextImpl::GetDataPath(
    const storage::BucketLocator& bucket_locator) const {
  // TODO(crbug.com/1315371): Allow custom bucket names.
  if (bucket_locator.storage_key.IsFirstPartyContext()) {
    // First-party idb files, for legacy reasons, are stored at:
    // {{storage_partition_path}}/IndexedDB/
    // TODO(crbug.com/1315371): Migrate all first party buckets to the new path.
    return GetFirstPartyDataPath();
  } else {
    // Third-party idb files are stored at:
    // {{storage_partition_path}}/WebStorage/
    // TODO(crbug.com/1315371): Use QuotaManagerProxy::GetBucketPath.
    return GetThirdPartyDataPath();
  }
}

const base::FilePath IndexedDBContextImpl::GetFirstPartyDataPath() const {
  return base_data_path_.empty()
             ? base_data_path_
             : base_data_path_.Append(storage::kIndexedDbDirectory);
}

const base::FilePath IndexedDBContextImpl::GetFirstPartyDataPathForTesting()
    const {
  return GetFirstPartyDataPath();
}

const base::FilePath IndexedDBContextImpl::GetThirdPartyDataPath() const {
  return base_data_path_.empty()
             ? base_data_path_
             : base_data_path_.Append(storage::kWebStorageDirectory);
}

void IndexedDBContextImpl::FactoryOpened(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  const auto& it =
      storage_key_to_bucket_locator_.find(bucket_locator.storage_key);
  if (it == storage_key_to_bucket_locator_.end()) {
    storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
    bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  }
  if (bucket_set_.insert(bucket_locator).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
  } else {
    EnsureDiskUsageCacheInitialized(bucket_locator);
  }
}

void IndexedDBContextImpl::ConnectionOpened(
    const storage::BucketLocator& bucket_locator,
    IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyBucketAccessed(bucket_locator.id,
                                              base::Time::Now());
  const auto& it =
      storage_key_to_bucket_locator_.find(bucket_locator.storage_key);
  if (it == storage_key_to_bucket_locator_.end()) {
    storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
    bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  }
  if (bucket_set_.insert(bucket_locator).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
  } else {
    EnsureDiskUsageCacheInitialized(bucket_locator);
  }
}

void IndexedDBContextImpl::ConnectionClosed(
    const storage::BucketLocator& bucket_locator,
    IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyBucketAccessed(bucket_locator.id,
                                              base::Time::Now());
  if (indexeddb_factory_.get() &&
      indexeddb_factory_->GetConnectionCount(bucket_locator) == 0)
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::TransactionComplete(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(!indexeddb_factory_.get() ||
         indexeddb_factory_->GetConnectionCount(bucket_locator) > 0);
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::DatabaseDeleted(
    const storage::BucketLocator& bucket_locator) {
  const auto& it =
      storage_key_to_bucket_locator_.find(bucket_locator.storage_key);
  if (it == storage_key_to_bucket_locator_.end()) {
    storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
    bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
  }
  bucket_set_.insert(bucket_locator);
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::BlobFilesCleaned(
    const storage::BucketLocator& bucket_locator) {
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::NotifyIndexedDBListChanged(
    const storage::BucketLocator& bucket_locator) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBListChanged(bucket_locator);
  }
}

void IndexedDBContextImpl::NotifyIndexedDBContentChanged(
    const storage::BucketLocator& bucket_locator,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBContentChanged(bucket_locator, database_name,
                                        object_store_name);
  }
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (indexeddb_factory_.get())
    indexeddb_factory_->ContextDestroyed();
}

void IndexedDBContextImpl::ShutdownOnIDBSequence() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (force_keep_session_state_)
    return;

  // Clear session-only databases.
  if (sites_to_purge_on_shutdown_.empty())
    return;

  IndexedDBFactoryImpl* factory = GetIDBFactory();
  const auto& storage_key_to_file_path =
      DefaultBucketFilePerFirstPartyStorageKey(GetFirstPartyDataPath());
  const auto& bucket_id_to_file_path =
      DefaultBucketFilePerThirdPartyBucketId(GetThirdPartyDataPath());
  for (const auto& bucket_locator : bucket_set_) {
    const auto& origin_it = sites_to_purge_on_shutdown_.find(
        net::SchemefulSite(bucket_locator.storage_key.origin()));
    const auto& top_site_it = sites_to_purge_on_shutdown_.find(
        bucket_locator.storage_key.top_level_site());
    if (origin_it == sites_to_purge_on_shutdown_.end() &&
        top_site_it == sites_to_purge_on_shutdown_.end()) {
      // No match for a site we want to clear in this bucket locator.
      continue;
    }
    base::FilePath path;
    const auto& first_party_it =
        storage_key_to_file_path.find(bucket_locator.storage_key);
    const auto& third_party_it = bucket_id_to_file_path.find(bucket_locator.id);
    // If the bucket exists on the file system, it is in only one of the two
    // possible locations depending on if the key is first or third party.
    if (first_party_it != storage_key_to_file_path.end()) {
      DCHECK(third_party_it == bucket_id_to_file_path.end());
      DCHECK(bucket_locator.storage_key.IsFirstPartyContext());
      path = first_party_it->second;
    } else if (third_party_it != bucket_id_to_file_path.end()) {
      DCHECK(first_party_it == storage_key_to_file_path.end());
      DCHECK(bucket_locator.storage_key.IsThirdPartyContext());
      path = third_party_it->second;
    }
    if (!path.empty()) {
      factory->ForceClose(bucket_locator, false);
      filesystem_proxy_->DeletePathRecursively(path);
    }
  }
}

void IndexedDBContextImpl::Shutdown() {
  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (is_incognito())
    return;

  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBContextImpl::InitializeFromFilesIfNeeded,
          weak_factory_.GetWeakPtr(),
          base::BindOnce(&IndexedDBContextImpl::ShutdownOnIDBSequence,
                         base::WrapRefCounted(this))));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const storage::BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetBlobStoreFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const storage::BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetLevelDBFileName(bucket_locator));
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(
    const storage::BucketLocator& bucket_locator) const {
  if (is_incognito()) {
    if (!indexeddb_factory_)
      return 0;
    return indexeddb_factory_->GetInMemoryDBSize(bucket_locator);
  }

  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(bucket_locator))
    total_size += filesystem_proxy_->ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const storage::BucketLocator& bucket_locator) {
  if (bucket_size_map_.find(bucket_locator) == bucket_size_map_.end())
    bucket_size_map_[bucket_locator] = ReadUsageFromDisk(bucket_locator);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(
    const storage::BucketLocator& bucket_locator) {
  int64_t former_disk_usage = bucket_size_map_[bucket_locator];
  int64_t current_disk_usage = ReadUsageFromDisk(bucket_locator);
  int64_t difference = current_disk_usage - former_disk_usage;
  if (difference) {
    bucket_size_map_[bucket_locator] = current_disk_usage;
    quota_manager_proxy()->NotifyBucketModified(
        storage::QuotaClientType::kIndexedDatabase, bucket_locator.id,
        difference, base::Time::Now(), base::SequencedTaskRunnerHandle::Get(),
        base::DoNothing());
    NotifyIndexedDBListChanged(bucket_locator);
  }
}

void IndexedDBContextImpl::InitializeFromFilesIfNeeded(
    base::OnceClosure callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (did_initialize_from_files_) {
    std::move(callback).Run();
    return;
  }
  const auto& storage_key_to_file_path =
      DefaultBucketFilePerFirstPartyStorageKey(GetFirstPartyDataPath());
  const auto& bucket_id_to_file_path =
      DefaultBucketFilePerThirdPartyBucketId(GetThirdPartyDataPath());
  if (storage_key_to_file_path.empty() && bucket_id_to_file_path.empty()) {
    did_initialize_from_files_ = true;
    std::move(callback).Run();
    return;
  }
  for (auto iter = storage_key_to_file_path.begin();
       iter != storage_key_to_file_path.end(); ++iter) {
    if (std::next(iter) == storage_key_to_file_path.end() &&
        bucket_id_to_file_path.empty()) {
      // `callback` is only passed to the last task scheduled (if the second
      // loop is empty)so that it won't be triggered until the update is
      // complete.
      GetOrCreateDefaultBucket(
          iter->first,
          base::BindOnce(
              [](base::WeakPtr<IndexedDBContextImpl> context,
                 base::OnceClosure inner_callback,
                 const absl::optional<storage::BucketLocator>& bucket_locator) {
                if (context) {
                  if (bucket_locator)
                    context->bucket_set_.insert(*bucket_locator);
                  context->did_initialize_from_files_ = true;
                }
                std::move(inner_callback).Run();
              },
              weak_factory_.GetWeakPtr(), std::move(callback)));
      // This path is only invoked once but the analyzer doesn't know that.
      callback = base::DoNothing();
    } else {
      GetOrCreateDefaultBucket(
          iter->first,
          base::BindOnce(
              [](base::WeakPtr<IndexedDBContextImpl> context,
                 const absl::optional<storage::BucketLocator>& bucket_locator) {
                if (bucket_locator && context)
                  context->bucket_set_.insert(*bucket_locator);
              },
              weak_factory_.GetWeakPtr()));
    }
  }
  for (auto iter = bucket_id_to_file_path.begin();
       iter != bucket_id_to_file_path.end(); ++iter) {
    if (std::next(iter) == bucket_id_to_file_path.end()) {
      // `callback` is only passed to the last task scheduled so that it won't
      // be triggered until the update is complete.
      GetBucketById(
          iter->first,
          base::BindOnce(
              [](base::WeakPtr<IndexedDBContextImpl> context,
                 base::OnceClosure inner_callback,
                 const absl::optional<storage::BucketLocator>& bucket_locator) {
                if (context) {
                  if (bucket_locator)
                    context->bucket_set_.insert(*bucket_locator);
                  context->did_initialize_from_files_ = true;
                }
                std::move(inner_callback).Run();
              },
              weak_factory_.GetWeakPtr(), std::move(callback)));
      // This path is only invoked once but the analyzer doesn't know that.
      callback = base::DoNothing();
    } else {
      GetBucketById(
          iter->first,
          base::BindOnce(
              [](base::WeakPtr<IndexedDBContextImpl> context,
                 const absl::optional<storage::BucketLocator>& bucket_locator) {
                if (bucket_locator && context)
                  context->bucket_set_.insert(*bucket_locator);
              },
              weak_factory_.GetWeakPtr()));
    }
  }
}

void IndexedDBContextImpl::ForceInitializeFromFilesForTesting(
    ForceInitializeFromFilesForTestingCallback callback) {
  did_initialize_from_files_ = false;
  InitializeFromFilesIfNeeded(std::move(callback));
}

void IndexedDBContextImpl::RegisterBucketLocatorToSkipQuotaLookupForTesting(
    const storage::BucketLocator& bucket_locator) {
  storage_key_to_bucket_locator_[bucket_locator.storage_key] = bucket_locator;
  bucket_id_to_bucket_locator_[bucket_locator.id] = bucket_locator;
}

void IndexedDBContextImpl::GetOrCreateDefaultBucket(
    const blink::StorageKey& storage_key,
    DidGetBucketLocatorCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  const auto& bucket_locator = storage_key_to_bucket_locator_.find(storage_key);
  if (bucket_locator == storage_key_to_bucket_locator_.end()) {
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        idb_task_runner_,
        base::BindOnce(
            [](base::WeakPtr<IndexedDBContextImpl> context,
               DidGetBucketLocatorCallback inner_callback,
               storage::QuotaErrorOr<storage::BucketInfo> result) {
              if (result.ok()) {
                const auto& bucket_locator = result->ToBucketLocator();
                if (context) {
                  context->storage_key_to_bucket_locator_[bucket_locator
                                                              .storage_key] =
                      bucket_locator;
                  context->bucket_id_to_bucket_locator_[bucket_locator.id] =
                      bucket_locator;
                }
                std::move(inner_callback).Run(bucket_locator);
              } else {
                std::move(inner_callback).Run(absl::nullopt);
              }
            },
            weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(bucket_locator->second);
  }
}

void IndexedDBContextImpl::GetBucketById(const storage::BucketId& bucket_id,
                                         DidGetBucketLocatorCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  const auto& bucket_locator = bucket_id_to_bucket_locator_.find(bucket_id);
  if (bucket_locator == bucket_id_to_bucket_locator_.end()) {
    quota_manager_proxy_->GetBucketById(
        bucket_id, idb_task_runner_,
        base::BindOnce(
            [](base::WeakPtr<IndexedDBContextImpl> context,
               DidGetBucketLocatorCallback inner_callback,
               storage::QuotaErrorOr<storage::BucketInfo> result) {
              if (result.ok()) {
                const auto& bucket_locator = result->ToBucketLocator();
                if (context) {
                  context->storage_key_to_bucket_locator_[bucket_locator
                                                              .storage_key] =
                      bucket_locator;
                  context->bucket_id_to_bucket_locator_[bucket_locator.id] =
                      bucket_locator;
                }
                std::move(inner_callback).Run(bucket_locator);
              } else {
                std::move(inner_callback).Run(absl::nullopt);
              }
            },
            weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(bucket_locator->second);
  }
}

}  // namespace content
