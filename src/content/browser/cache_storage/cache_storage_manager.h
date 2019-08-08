// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "storage/browser/quota/quota_client.h"

namespace storage {
class BlobStorageContext;
}

namespace url {
class Origin;
}

namespace content {

enum class CacheStorageOwner {
  kMinValue,

  // Caches that can be accessed by the JS CacheStorage API (developer facing).
  kCacheAPI = kMinValue,

  // Private cache to store background fetch downloads.
  kBackgroundFetch,

  kMaxValue = kBackgroundFetch
};

// Keeps track of a CacheStorage per origin. There is one
// CacheStorageManager per ServiceWorkerContextCore.
// TODO(jkarlin): Remove CacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT CacheStorageManager
    : public base::RefCountedThreadSafe<CacheStorageManager,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  // Open the CacheStorage for the given origin and owner.  A reference counting
  // handle is returned which can be stored and used similar to a weak pointer.
  virtual CacheStorageHandle OpenCacheStorage(const url::Origin& origin,
                                              CacheStorageOwner owner) = 0;

  // QuotaClient and Browsing Data Deletion support.
  virtual void GetAllOriginsUsage(
      CacheStorageOwner owner,
      CacheStorageContext::GetUsageInfoCallback callback) = 0;
  virtual void GetOriginUsage(
      const url::Origin& origin_url,
      CacheStorageOwner owner,
      storage::QuotaClient::GetUsageCallback callback) = 0;
  virtual void GetOrigins(
      CacheStorageOwner owner,
      storage::QuotaClient::GetOriginsCallback callback) = 0;
  virtual void GetOriginsForHost(
      const std::string& host,
      CacheStorageOwner owner,
      storage::QuotaClient::GetOriginsCallback callback) = 0;
  virtual void DeleteOriginData(
      const url::Origin& origin,
      CacheStorageOwner owner,
      storage::QuotaClient::DeletionCallback callback) = 0;
  virtual void DeleteOriginData(const url::Origin& origin,
                                CacheStorageOwner owner) = 0;

  // This must be called before any of the public Cache functions above.
  virtual void SetBlobParametersForCache(
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context) = 0;

  static bool IsValidQuotaOrigin(const url::Origin& origin);

 protected:
  friend class base::DeleteHelper<CacheStorageManager>;
  friend class base::RefCountedThreadSafe<CacheStorageManager>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  virtual ~CacheStorageManager() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
