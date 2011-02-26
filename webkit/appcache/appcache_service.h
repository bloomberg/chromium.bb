// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
#define WEBKIT_APPCACHE_APPCACHE_SERVICE_H_

#include <map>
#include <set>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
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
class SpecialStoragePolicy;
}

namespace appcache {

class AppCacheBackendImpl;
class AppCachePolicy;

// Refcounted container to avoid copying the collection in callbacks.
struct AppCacheInfoCollection
    : public base::RefCountedThreadSafe<AppCacheInfoCollection> {
  AppCacheInfoCollection();
  virtual ~AppCacheInfoCollection();

  std::map<GURL, AppCacheInfoVector> infos_by_origin;
};

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile. Each instance has
// exclusive access to it's cache_directory on disk.
class AppCacheService {
 public:
  AppCacheService();
  virtual ~AppCacheService();

  void Initialize(const FilePath& cache_directory,
                  base::MessageLoopProxy* cache_thread);

  // Purges any memory not needed.
  void PurgeMemory() {
    if (storage_.get())
      storage_->PurgeMemory();
  }

  // Determines if a request for 'url' can be satisfied while offline.
  // This method always completes asynchronously.
  void CanHandleMainResourceOffline(const GURL& url,
                                    net::CompletionCallback* callback);

  // Populates 'collection' with info about all of the appcaches stored
  // within the service, 'callback' is invoked upon completion. The service
  // acquires a reference to the 'collection' until until completion.
  // This method always completes asynchronously.
  void GetAllAppCacheInfo(AppCacheInfoCollection* collection,
                          net::CompletionCallback* callback);

  // Deletes the group identified by 'manifest_url', 'callback' is
  // invoked upon completion. Upon completion, the cache group and
  // any resources within the group are no longer loadable and all
  // subresource loads for pages associated with a deleted group
  // will fail. This method always completes asynchronously.
  void DeleteAppCacheGroup(const GURL& manifest_url,
                           net::CompletionCallback* callback);

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

  // Each child process in chrome uses a distinct backend instance.
  // See chrome/browser/AppCacheDispatcherHost.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) const {
    BackendMap::const_iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  AppCacheStorage* storage() const { return storage_.get(); }

 protected:
  class AsyncHelper;
  class CanHandleOfflineHelper;
  class DeleteHelper;
  class GetInfoHelper;

  typedef std::set<AsyncHelper*> PendingAsyncHelpers;
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;

  AppCachePolicy* appcache_policy_;
  scoped_ptr<AppCacheStorage> storage_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  PendingAsyncHelpers pending_helpers_;
  BackendMap backends_;  // One 'backend' per child process.
  // Context for use during cache updates.
  net::URLRequestContext* request_context_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheService);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
