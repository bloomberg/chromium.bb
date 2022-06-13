// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"

namespace storage {
class QuotaOverrideHandle;
}

namespace content {
class StoragePartition;

namespace protocol {

class StorageHandler : public DevToolsDomainHandler,
                       public Storage::Backend {
 public:
  StorageHandler();

  StorageHandler(const StorageHandler&) = delete;
  StorageHandler& operator=(const StorageHandler&) = delete;

  ~StorageHandler() override;

  // content::protocol::DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  // content::protocol::storage::Backend
  void ClearDataForOrigin(
      const std::string& origin,
      const std::string& storage_types,
      std::unique_ptr<ClearDataForOriginCallback> callback) override;
  void GetUsageAndQuota(
      const String& origin,
      std::unique_ptr<GetUsageAndQuotaCallback> callback) override;

  // Storage Quota Override
  void GetQuotaOverrideHandle();
  void OverrideQuotaForOrigin(
      const String& origin,
      Maybe<double> quota_size,
      std::unique_ptr<OverrideQuotaForOriginCallback> callback) override;

  // Cookies management
  void GetCookies(
      Maybe<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::GetCookiesCallback> callback) override;

  void SetCookies(
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      Maybe<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::SetCookiesCallback> callback) override;

  void ClearCookies(Maybe<std::string> browser_context_id,
                    std::unique_ptr<Storage::Backend::ClearCookiesCallback>
                        callback) override;

  // Ignores all double calls to track an origin.
  Response TrackCacheStorageForOrigin(const std::string& origin) override;
  Response UntrackCacheStorageForOrigin(const std::string& origin) override;
  Response TrackIndexedDBForOrigin(const std::string& origin) override;
  Response UntrackIndexedDBForOrigin(const std::string& origin) override;

  void GetTrustTokens(
      std::unique_ptr<GetTrustTokensCallback> callback) override;
  void ClearTrustTokens(
      const std::string& issuerOrigin,
      std::unique_ptr<ClearTrustTokensCallback> callback) override;

 private:
  // See definition for lifetime information.
  class CacheStorageObserver;
  class IndexedDBObserver;

  // Not thread safe.
  CacheStorageObserver* GetCacheStorageObserver();
  IndexedDBObserver* GetIndexedDBObserver();

  void NotifyCacheStorageListChanged(const std::string& origin);
  void NotifyCacheStorageContentChanged(const std::string& origin,
                                        const std::string& name);
  void NotifyIndexedDBListChanged(const std::string& origin);
  void NotifyIndexedDBContentChanged(const std::string& origin,
                                     const std::u16string& database_name,
                                     const std::u16string& object_store_name);

  Response FindStoragePartition(const Maybe<std::string>& browser_context_id,
                                StoragePartition** storage_partition);

  std::unique_ptr<Storage::Frontend> frontend_;
  StoragePartition* storage_partition_;
  std::unique_ptr<CacheStorageObserver> cache_storage_observer_;
  std::unique_ptr<IndexedDBObserver> indexed_db_observer_;

  // Exposes the API for managing storage quota overrides.
  std::unique_ptr<storage::QuotaOverrideHandle> quota_override_handle_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
