// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

namespace cache_storage_manager_unittest {
class CacheStorageManagerTest;
}

// A concrete implementation of the CacheStorageManager interface using
// the legacy disk_cache backend.
class CONTENT_EXPORT LegacyCacheStorageManager : public CacheStorageManager {
 public:
  static scoped_refptr<LegacyCacheStorageManager> Create(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  // Create a new manager using the underlying configuration of the given
  // manager, but with its own list of storage objects.  This is only used
  // for testing.
  static scoped_refptr<LegacyCacheStorageManager> CreateForTesting(
      LegacyCacheStorageManager* old_manager);

  LegacyCacheStorageManager(const LegacyCacheStorageManager&) = delete;
  LegacyCacheStorageManager& operator=(const LegacyCacheStorageManager&) =
      delete;

  // Map a database identifier (computed from a storage key) to the path.
  static base::FilePath ConstructStorageKeyPath(
      const base::FilePath& root_path,
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner);

  // Open the CacheStorage for the given storage_key and owner.  A reference
  // counting handle is returned which can be stored and used similar to a weak
  // pointer.
  CacheStorageHandle OpenCacheStorage(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner) override;

  void GetAllStorageKeysUsage(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback
          callback) override;
  void GetStorageKeyUsage(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetStorageKeyUsageCallback callback)
      override;
  void GetStorageKeys(storage::mojom::CacheStorageOwner owner,
                      storage::mojom::QuotaClient::GetStorageKeysForTypeCallback
                          callback) override;
  void GetStorageKeysForHost(
      const std::string& host,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetStorageKeysForHostCallback callback)
      override;
  void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteStorageKeyDataCallback callback)
      override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            storage::mojom::CacheStorageOwner owner) override;
  void AddObserver(mojo::PendingRemote<storage::mojom::CacheStorageObserver>
                       observer) override;

  void NotifyCacheListChanged(const blink::StorageKey& storage_key);
  void NotifyCacheContentChanged(const blink::StorageKey& storage_key,
                                 const std::string& name);

  base::FilePath root_path() const { return root_path_; }

  // This method is called when the last CacheStorageHandle for a particular
  // instance is destroyed and its reference count drops to zero.
  void CacheStorageUnreferenced(LegacyCacheStorage* cache_storage,
                                const blink::StorageKey& storage_key,
                                storage::mojom::CacheStorageOwner owner);

 private:
  friend class cache_storage_manager_unittest::CacheStorageManagerTest;
  friend class CacheStorageContextImpl;

  typedef std::map<
      std::pair<blink::StorageKey, storage::mojom::CacheStorageOwner>,
      std::unique_ptr<LegacyCacheStorage>>
      CacheStorageMap;

  LegacyCacheStorageManager(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  ~LegacyCacheStorageManager() override;

  void GetAllStorageKeysUsageGetSizes(
      storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback
          callback,
      std::vector<storage::mojom::StorageUsageInfoPtr> usage_info);

  void DeleteStorageKeyDidClose(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteStorageKeyDataCallback callback,
      std::unique_ptr<LegacyCacheStorage> cache_storage,
      int64_t origin_size);

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner() const {
    return cache_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

  bool IsMemoryBacked() const { return root_path_.empty(); }

  // MemoryPressureListener callback
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  base::FilePath root_path_;
  const scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The map owns the CacheStorages and the CacheStorages are only accessed on
  // |cache_task_runner_|.
  CacheStorageMap cache_storage_map_;

  mojo::RemoteSet<storage::mojom::CacheStorageObserver> observers_;

  const scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_
