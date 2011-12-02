// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
#define WEBKIT_APPCACHE_APPCACHE_SERVICE_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_storage.h"

class FilePath;

namespace net {
class URLRequestContext;
}  // namespace net

namespace base {
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace appcache {

class AppCacheBackendImpl;
class AppCacheQuotaClient;
class AppCachePolicy;

// Refcounted container to avoid copying the collection in callbacks.
struct APPCACHE_EXPORT AppCacheInfoCollection
    : public base::RefCountedThreadSafe<AppCacheInfoCollection> {
  AppCacheInfoCollection();
  virtual ~AppCacheInfoCollection();

  std::map<GURL, AppCacheInfoVector> infos_by_origin;
};

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile. Each instance has
// exclusive access to it's cache_directory on disk.
class APPCACHE_EXPORT AppCacheService {
 public:
  // If not using quota management, the proxy may be NULL.
  explicit AppCacheService(quota::QuotaManagerProxy* quota_manager_proxy);
  virtual ~AppCacheService();

  void Initialize(const FilePath& cache_directory,
                  base::MessageLoopProxy* db_thread,
                  base::MessageLoopProxy* cache_thread);

  // Purges any memory not needed.
  void PurgeMemory() {
    if (storage_.get())
      storage_->PurgeMemory();
  }

  // Determines if a request for 'url' can be satisfied while offline.
  // This method always completes asynchronously.
  void CanHandleMainResourceOffline(const GURL& url,
                                    const GURL& first_party,
                                    const net::CompletionCallback& callback);

  // Populates 'collection' with info about all of the appcaches stored
  // within the service, 'callback' is invoked upon completion. The service
  // acquires a reference to the 'collection' until until completion.
  // This method always completes asynchronously.
  void GetAllAppCacheInfo(AppCacheInfoCollection* collection,
                          const net::CompletionCallback& callback);

  // Deletes the group identified by 'manifest_url', 'callback' is
  // invoked upon completion. Upon completion, the cache group and
  // any resources within the group are no longer loadable and all
  // subresource loads for pages associated with a deleted group
  // will fail. This method always completes asynchronously.
  void DeleteAppCacheGroup(const GURL& manifest_url,
                           net::OldCompletionCallback* callback);

  // Deletes all appcaches for the origin, 'callback' is invoked upon
  // completion. This method always completes asynchronously.
  // (virtual for unittesting)
  virtual void DeleteAppCachesForOrigin(const GURL& origin,
                                        net::OldCompletionCallback* callback);

  // Checks the integrity of 'response_id' by reading the headers and data.
  // If it cannot be read, the cache group for 'manifest_url' is deleted.
  void CheckAppCacheResponse(const GURL& manifest_url_, int64 cache_id,
                             int64 response_id);

  // Context for use during cache updates, should only be accessed
  // on the IO thread. We do NOT add a reference to the request context,
  // it is the callers responsibility to ensure that the pointer
  // remains valid while set.
  net::URLRequestContext* request_context() const { return request_context_; }
  void set_request_context(net::URLRequestContext* context) {
    request_context_ = context;
  }

  // The appcache policy, may be null, in which case access is always allowed.
  // The service does NOT assume ownership of the policy, it is the callers
  // responsibility to ensure that the pointer remains valid while set.
  AppCachePolicy* appcache_policy() const { return appcache_policy_; }
  void set_appcache_policy(AppCachePolicy* policy) {
    appcache_policy_ = policy;
  }

  quota::SpecialStoragePolicy* special_storage_policy() const {
    return special_storage_policy_.get();
  }
  void set_special_storage_policy(quota::SpecialStoragePolicy* policy);

  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  AppCacheQuotaClient* quota_client() const {
    return quota_client_;
  }

  // Each child process in chrome uses a distinct backend instance.
  // See chrome/browser/AppCacheDispatcherHost.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) const {
    BackendMap::const_iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  AppCacheStorage* storage() const { return storage_.get(); }

  bool clear_local_state_on_exit() const { return clear_local_state_on_exit_; }
  void set_clear_local_state_on_exit(bool clear_local_state_on_exit) {
    clear_local_state_on_exit_ = clear_local_state_on_exit; }

 protected:
  friend class AppCacheStorageImplTest;
  friend class AppCacheServiceTest;

  class AsyncHelper;
  class NewAsyncHelper;
  class CanHandleOfflineHelper;
  class DeleteHelper;
  class DeleteOriginHelper;
  class GetInfoHelper;
  class CheckResponseHelper;

  typedef std::set<AsyncHelper*> PendingAsyncHelpers;
  typedef std::set<NewAsyncHelper*> PendingNewAsyncHelpers;
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;

  AppCachePolicy* appcache_policy_;
  AppCacheQuotaClient* quota_client_;
  scoped_ptr<AppCacheStorage> storage_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  PendingAsyncHelpers pending_helpers_;
  PendingNewAsyncHelpers pending_new_helpers_;
  BackendMap backends_;  // One 'backend' per child process.
  // Context for use during cache updates.
  net::URLRequestContext* request_context_;
  bool clear_local_state_on_exit_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheService);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
