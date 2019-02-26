// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/common/content_export.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

enum class CacheStorageOwner;
struct ServiceWorkerFetchRequest;

// The state needed to pass when writing to a cache.
struct PutContext {
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;

  PutContext(std::unique_ptr<ServiceWorkerFetchRequest> request,
             blink::mojom::FetchAPIResponsePtr response,
             blink::mojom::BlobPtr blob,
             uint64_t blob_size,
             blink::mojom::BlobPtr side_data_blob,
             uint64_t side_data_blob_size);

  ~PutContext();

  // Provided by the constructor.
  std::unique_ptr<ServiceWorkerFetchRequest> request;
  blink::mojom::FetchAPIResponsePtr response;
  blink::mojom::BlobPtr blob;
  uint64_t blob_size;
  blink::mojom::BlobPtr side_data_blob;
  uint64_t side_data_blob_size;

  // Provided while writing to the cache.
  ErrorCallback callback;
  disk_cache::ScopedEntryPtr cache_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

class CONTENT_EXPORT CacheStorageCacheEntryHandler {
 public:
  // This class ensures that the cache and the entry have a lifetime as long as
  // the blob that is created to contain them.
  class BlobDataHandle : public storage::BlobDataBuilder::DataHandle {
   public:
    BlobDataHandle(base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
                   CacheStorageCacheHandle cache_handle,
                   disk_cache::ScopedEntryPtr entry);

    bool IsValid() override;

    void Invalidate();

   private:
    ~BlobDataHandle() override;

    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler_;
    base::Optional<CacheStorageCacheHandle> cache_handle_;
    disk_cache::ScopedEntryPtr entry_;

    DISALLOW_COPY_AND_ASSIGN(BlobDataHandle);
  };

  virtual ~CacheStorageCacheEntryHandler();

  virtual std::unique_ptr<PutContext> CreatePutContext(
      std::unique_ptr<ServiceWorkerFetchRequest>,
      blink::mojom::FetchAPIResponsePtr response) = 0;
  virtual void PopulateResponseBody(
      CacheStorageCacheHandle handle,
      disk_cache::ScopedEntryPtr entry,
      blink::mojom::FetchAPIResponse* response) = 0;
  virtual void PopulateRequestBody(CacheStorageCacheHandle handle,
                                   disk_cache::ScopedEntryPtr entry,
                                   blink::mojom::FetchAPIRequest* request) = 0;

  static std::unique_ptr<CacheStorageCacheEntryHandler> CreateCacheEntryHandler(
      CacheStorageOwner owner,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  void InvalidateBlobDataHandles();

  void EraseBlobDataHandle(BlobDataHandle* handle);

 protected:
  CacheStorageCacheEntryHandler(
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  // We keep track of the BlobDataHandle instances to allow us to invalidate
  // them if the cache has to be deleted while there are still references to
  // data in it.
  std::set<BlobDataHandle*> blob_data_handles_;

  base::WeakPtrFactory<CacheStorageCacheEntryHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCacheEntryHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
