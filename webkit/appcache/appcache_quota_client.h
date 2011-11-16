// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
#define WEBKIT_APPCACHE_APPCACHE_QUOTA_CLIENT_H_

#include <deque>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace appcache {

class AppCacheService;
class AppCacheStorageImpl;
class AppCacheQuotaClientTest;

// A QuotaClient implementation to integrate the appcache service
// with the quota management system. The QuotaClient interface is
// used on the IO thread by the quota manager. This class deletes
// itself when both the quota manager and the appcache service have
// been destroyed.
class AppCacheQuotaClient : public quota::QuotaClient {
 public:
  typedef std::deque<base::Closure> RequestQueue;

  virtual ~AppCacheQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin,
                              quota::StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(quota::StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(quota::StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                quota::StorageType type,
                                const DeletionCallback& callback) OVERRIDE;

 private:
  friend class AppCacheService;  // for NotifyAppCacheIsDestroyed
  friend class AppCacheStorageImpl;  // for NotifyAppCacheIsReady
  friend class AppCacheQuotaClientTest;

  APPCACHE_EXPORT explicit AppCacheQuotaClient(AppCacheService* service);

  void DidDeleteAppCachesForOrigin(int rv);
  void GetOriginsHelper(quota::StorageType type,
                        const std::string& opt_host,
                        const GetOriginsCallback& callback);
  void ProcessPendingRequests();
  void DeletePendingRequests();
  const AppCacheStorage::UsageMap* GetUsageMap();

  // For use by appcache internals during initialization and shutdown.
  APPCACHE_EXPORT void NotifyAppCacheReady();
  APPCACHE_EXPORT void NotifyAppCacheDestroyed();

  // Prior to appcache service being ready, we have to queue
  // up reqeusts and defer acting on them until we're ready.
  RequestQueue pending_batch_requests_;
  RequestQueue pending_serial_requests_;

  // And once it's ready, we can only handle one delete request at a time,
  // so we queue up additional requests while one is in already in progress.
  DeletionCallback current_delete_request_callback_;
  scoped_refptr<net::CancelableOldCompletionCallback<AppCacheQuotaClient> >
      service_delete_callback_;

  AppCacheService* service_;
  bool appcache_is_ready_;
  bool quota_manager_is_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheQuotaClient);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
