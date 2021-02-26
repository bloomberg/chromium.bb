// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

CacheStorageQuotaClient::CacheStorageQuotaClient(
    scoped_refptr<CacheStorageManager> cache_manager,
    CacheStorageOwner owner)
    : cache_manager_(std::move(cache_manager)), owner_(owner) {}

CacheStorageQuotaClient::~CacheStorageQuotaClient() = default;

void CacheStorageQuotaClient::OnQuotaManagerDestroyed() {}

void CacheStorageQuotaClient::GetOriginUsage(const url::Origin& origin,
                                             blink::mojom::StorageType type,
                                             GetOriginUsageCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaOrigin(origin)) {
    std::move(callback).Run(0);
    return;
  }

  cache_manager_->GetOriginUsage(origin, owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetOriginsForType(
    blink::mojom::StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  cache_manager_->GetOrigins(owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  cache_manager_->GetOriginsForHost(host, owner_, std::move(callback));
}

void CacheStorageQuotaClient::DeleteOriginData(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    DeleteOriginDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaOrigin(origin)) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  cache_manager_->DeleteOriginData(origin, owner_, std::move(callback));
}

void CacheStorageQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  std::move(callback).Run();
}

// static
storage::QuotaClientType CacheStorageQuotaClient::GetClientTypeFromOwner(
    CacheStorageOwner owner) {
  switch (owner) {
    case CacheStorageOwner::kCacheAPI:
      return storage::QuotaClientType::kServiceWorkerCache;
    case CacheStorageOwner::kBackgroundFetch:
      return storage::QuotaClientType::kBackgroundFetch;
  }
}

}  // namespace content
