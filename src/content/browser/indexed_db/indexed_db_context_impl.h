// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
class Value;
}  // namespace base

namespace storage {
class QuotaClientCallbackWrapper;
}  // namespace storage

namespace content {
class IndexedDBConnection;
class IndexedDBFactoryImpl;
class IndexedDBQuotaClient;

class CONTENT_EXPORT IndexedDBContextImpl
    : public base::RefCountedThreadSafe<IndexedDBContextImpl>,
      public storage::mojom::IndexedDBControl,
      public storage::mojom::IndexedDBControlTest {
 public:
  // Release `context` on the IDBTaskRunner.
  static void ReleaseOnIDBSequence(
      scoped_refptr<IndexedDBContextImpl>&& context);

  // If `base_data_path` is empty, nothing will be saved to disk.
  // `task_runner` is optional, and only set during testing.
  // This is *not* called on the IDBTaskRunner, unlike most other functions.
  IndexedDBContextImpl(
      const base::FilePath& base_data_path,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::Clock* clock,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  IndexedDBContextImpl(const IndexedDBContextImpl&) = delete;
  IndexedDBContextImpl& operator=(const IndexedDBContextImpl&) = delete;

  void Bind(mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForBucket(const blink::StorageKey& storage_key,
                       DeleteForBucketCallback callback) override;
  void DeleteForBucket(const storage::BucketLocator& bucket_locator,
                       DeleteForBucketCallback callback);
  void ForceClose(const blink::StorageKey& storage_key,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void ForceClose(const storage::BucketLocator& bucket_locator,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback);
  void GetConnectionCount(const blink::StorageKey& storage_key,
                          GetConnectionCountCallback callback) override;
  void GetConnectionCount(const storage::BucketLocator& bucket_locator,
                          GetConnectionCountCallback callback);
  void DownloadBucketData(const blink::StorageKey& storage_key,
                          DownloadBucketDataCallback callback) override;
  void DownloadBucketData(const storage::BucketLocator& bucket_locator,
                          DownloadBucketDataCallback callback);
  void GetAllBucketsDetails(GetAllBucketsDetailsCallback callback) override;
  void SetForceKeepSessionState() override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // mojom::IndexedDBControlTest implementation:
  void GetBaseDataPathForTesting(
      GetBaseDataPathForTestingCallback callback) override;
  void GetFilePathForTesting(const storage::BucketLocator& bucket_locator,
                             GetFilePathForTestingCallback callback) override;
  void ResetCachesForTesting(base::OnceClosure callback) override;
  void ForceSchemaDowngradeForTesting(
      const storage::BucketLocator& bucket_locator,
      ForceSchemaDowngradeForTestingCallback callback) override;
  void HasV2SchemaCorruptionForTesting(
      const storage::BucketLocator& bucket_locator,
      HasV2SchemaCorruptionForTestingCallback callback) override;
  void WriteToIndexedDBForTesting(const storage::BucketLocator& bucket_locator,
                                  const std::string& key,
                                  const std::string& value,
                                  base::OnceClosure callback) override;
  void GetBlobCountForTesting(const storage::BucketLocator& bucket_locator,
                              GetBlobCountForTestingCallback callback) override;
  void GetNextBlobNumberForTesting(
      const storage::BucketLocator& bucket_locator,
      int64_t database_id,
      GetNextBlobNumberForTestingCallback callback) override;
  void GetPathForBlobForTesting(
      const storage::BucketLocator& bucket_locator,
      int64_t database_id,
      int64_t blob_number,
      GetPathForBlobForTestingCallback callback) override;
  void CompactBackingStoreForTesting(
      const storage::BucketLocator& bucket_locator,
      base::OnceClosure callback) override;
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver)
      override;
  void GetDatabaseKeysForTesting(
      GetDatabaseKeysForTestingCallback callback) override;
  void ForceInitializeFromFilesForTesting(
      ForceInitializeFromFilesForTestingCallback callback) override;

  IndexedDBFactoryImpl* GetIDBFactory();

  // Called by StoragePartitionImpl to clear session-only data.
  // *not* called on the IDBTaskRunner.
  void Shutdown();

  int64_t GetBucketDiskUsage(const storage::BucketLocator& bucket_locator);

  // This getter is thread-safe.
  base::SequencedTaskRunner* IDBTaskRunner() { return idb_task_runner_.get(); }

  // Methods called by IndexedDBFactoryImpl or IndexedDBDispatcherHost for
  // quota support.
  void FactoryOpened(const storage::BucketLocator& bucket_locator);
  void ConnectionOpened(const storage::BucketLocator& bucket_locator,
                        IndexedDBConnection* db);
  void ConnectionClosed(const storage::BucketLocator& bucket_locator,
                        IndexedDBConnection* db);
  void TransactionComplete(const storage::BucketLocator& bucket_locator);
  void DatabaseDeleted(const storage::BucketLocator& bucket_locator);

  // Called when blob files have been cleaned (an aggregated delayed task).
  void BlobFilesCleaned(const storage::BucketLocator& bucket_locator);

  // Will be null in unit tests.
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  // Returns a list of all BucketLocators with backing stores.
  std::vector<storage::BucketLocator> GetAllBuckets();
  bool HasBucket(const storage::BucketLocator& bucket_locator);

  // Used by IndexedDBInternalsUI to populate internals page.
  base::Value* GetAllBucketsDetails();

  // GetStoragePaths returns all paths owned by this database, in arbitrary
  // order.
  std::vector<base::FilePath> GetStoragePaths(
      const storage::BucketLocator& bucket_locator) const;

  const base::FilePath GetDataPath(
      const storage::BucketLocator& bucket_locator) const;
  const base::FilePath GetFirstPartyDataPathForTesting() const;

  bool IsInMemoryContext() const { return base_data_path_.empty(); }
  size_t GetConnectionCountSync(const storage::BucketLocator& bucket_locator);
  int GetBucketBlobFileCount(const storage::BucketLocator& bucket_locator);

  bool is_incognito() const { return base_data_path_.empty(); }

  storage::mojom::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_ ? blob_storage_context_.get() : nullptr;
  }
  storage::mojom::FileSystemAccessContext* file_system_access_context() const {
    return file_system_access_context_ ? file_system_access_context_.get()
                                       : nullptr;
  }

  void NotifyIndexedDBListChanged(const storage::BucketLocator& bucket_locator);
  void NotifyIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name);

  // In unittests where the quota_manager_proxy is mocked, this is how you can
  // skip the bucket lookup and fake its existence.
  void RegisterBucketLocatorToSkipQuotaLookupForTesting(
      const storage::BucketLocator& bucket_locator);

 private:
  friend class base::RefCountedThreadSafe<IndexedDBContextImpl>;

  class IndexedDBGetUsageAndQuotaCallback;

  ~IndexedDBContextImpl() override;

  void BindPipesOnIDBSequence(
      mojo::PendingReceiver<storage::mojom::QuotaClient>
          pending_quota_client_receiver,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          pending_blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          pending_file_system_access_context);

  // mojom::IndexedDBControl internal implementation:
  void BindIndexedDBImpl(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      const absl::optional<storage::BucketLocator>& bucket_locator);
  void GetUsageImpl(GetUsageCallback usage_callback);
  void DeleteForBucketImpl(
      DeleteForBucketCallback callback,
      const absl::optional<storage::BucketLocator>& bucket_locator);
  void ForceCloseImpl(
      const storage::mojom::ForceCloseReason reason,
      base::OnceClosure closure,
      const absl::optional<storage::BucketLocator>& bucket_locator);
  void GetConnectionCountImpl(
      GetConnectionCountCallback callback,
      const absl::optional<storage::BucketLocator>& bucket_locator);
  void DownloadBucketDataImpl(
      DownloadBucketDataCallback callback,
      const absl::optional<storage::BucketLocator>& bucket_locator);

  void ShutdownOnIDBSequence();

  const base::FilePath GetFirstPartyDataPath() const;
  const base::FilePath GetThirdPartyDataPath() const;
  base::FilePath GetBlobStorePath(
      const storage::BucketLocator& bucket_locator) const;
  base::FilePath GetLevelDBPath(
      const storage::BucketLocator& bucket_locator) const;

  int64_t ReadUsageFromDisk(const storage::BucketLocator& bucket_locator) const;
  void EnsureDiskUsageCacheInitialized(
      const storage::BucketLocator& bucket_locator);
  // Compares the disk usage stored in `bucket_size_map_` with disk. If
  // there is a difference, it updates `bucket_size_map_` and notifies the
  // quota system.
  void QueryDiskAndUpdateQuotaUsage(
      const storage::BucketLocator& bucket_locator);
  base::Time GetBucketLastModified(
      const storage::BucketLocator& bucket_locator);

  // We need to initialize the buckets already stored to the disk, but this
  // cannot be done in the constructor as it might block destruction.
  void InitializeFromFilesIfNeeded(base::OnceClosure callback);
  bool did_initialize_from_files_{false};

  using DidGetBucketLocatorCallback = base::OnceCallback<void(
      const absl::optional<storage::BucketLocator>& bucket_locator)>;

  // This function provides an easy way to wrap the common operation: find
  // bucket in `storage_key_to_bucket_locator_` if it exists, otherwise look
  // it up in the `quota_manager_proxy_`, cache it, and then run the callback.
  void GetOrCreateDefaultBucket(const blink::StorageKey& storage_key,
                                DidGetBucketLocatorCallback callback);

  // This function provides an easy way to wrap the common operation: find
  // bucket in `bucket_id_to_bucket_locator_` if it exists, otherwise look
  // it up in the `quota_manager_proxy_`, cache it, and then run the callback.
  void GetBucketById(const storage::BucketId& bucket_id,
                     DidGetBucketLocatorCallback callback);

  // TODO(crbug.com/1315371): We need a way to map the StorageKey to a single
  // valid BucketLocator for legacy API purposes. This member should be removed
  // as it blocks the use of non-default named buckets.
  std::map<blink::StorageKey, storage::BucketLocator>
      storage_key_to_bucket_locator_;

  std::map<storage::BucketId, storage::BucketLocator>
      bucket_id_to_bucket_locator_;

  const scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;
  IndexedDBDispatcherHost dispatcher_host_;

  // Bound and accessed on the `idb_task_runner_`.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  mojo::Remote<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;
  std::unique_ptr<IndexedDBFactoryImpl> indexeddb_factory_;

  // If `base_data_path_` is empty then this is an incognito session and the
  // backing store will be held in-memory rather than on-disk.
  const base::FilePath base_data_path_;

  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  std::set<storage::BucketLocator> bucket_set_;
  std::map<storage::BucketLocator, int64_t> bucket_size_map_;
  // The set of sites whose storage should be cleared on shutdown. These are
  // matched against the origin and top level site in each bucket's StorageKey.
  std::set<net::SchemefulSite> sites_to_purge_on_shutdown_;
  const raw_ptr<base::Clock> clock_;

  const std::unique_ptr<IndexedDBQuotaClient> quota_client_;
  const std::unique_ptr<storage::QuotaClientCallbackWrapper>
      quota_client_wrapper_;

  mojo::ReceiverSet<storage::mojom::IndexedDBControl> receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBControlTest> test_receivers_;
  absl::optional<mojo::Receiver<storage::mojom::MockFailureInjector>>
      mock_failure_injector_;
  mojo::RemoteSet<storage::mojom::IndexedDBObserver> observers_;
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_;
  const std::unique_ptr<storage::FilesystemProxy> filesystem_proxy_;

  // weak_factory_->GetWeakPtr() may be used on any thread, but the resulting
  // pointer must only be checked/used on idb_task_runner_.
  base::WeakPtrFactory<IndexedDBContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
