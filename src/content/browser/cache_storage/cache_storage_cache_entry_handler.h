// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "content/common/content_export.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

enum class CacheStorageOwner;

// The state needed to pass when writing to a cache.
struct PutContext {
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;

  PutContext(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::FetchAPIResponsePtr response,
             blink::mojom::BlobPtr blob,
             uint64_t blob_size,
             blink::mojom::BlobPtr side_data_blob,
             uint64_t side_data_blob_size,
             int64_t trace_id);

  ~PutContext();

  // Provided by the constructor.
  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
  blink::mojom::BlobPtr blob;
  uint64_t blob_size;
  blink::mojom::BlobPtr side_data_blob;
  uint64_t side_data_blob_size;
  int64_t trace_id;

  // Provided while writing to the cache.
  ErrorCallback callback;
  ScopedWritableEntry cache_entry;

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

    disk_cache::ScopedEntryPtr& entry() { return entry_; }

   private:
    ~BlobDataHandle() override;

    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler_;
    base::Optional<CacheStorageCacheHandle> cache_handle_;
    disk_cache::ScopedEntryPtr entry_;

    DISALLOW_COPY_AND_ASSIGN(BlobDataHandle);
  };

  scoped_refptr<BlobDataHandle> CreateBlobDataHandle(
      CacheStorageCacheHandle cache_handle,
      disk_cache::ScopedEntryPtr entry);

  virtual ~CacheStorageCacheEntryHandler();

  virtual std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) = 0;
  virtual void PopulateResponseBody(
      scoped_refptr<BlobDataHandle> data_handle,
      blink::mojom::FetchAPIResponse* response) = 0;
  virtual void PopulateRequestBody(scoped_refptr<BlobDataHandle> data_handle,
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

  // Every subclass should provide its own implementation to avoid partial
  // destruction.
  virtual base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() = 0;

  // We keep track of the BlobDataHandle instances to allow us to invalidate
  // them if the cache has to be deleted while there are still references to
  // data in it.
  std::set<BlobDataHandle*> blob_data_handles_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCacheEntryHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
