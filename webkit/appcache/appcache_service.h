// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
#define WEBKIT_APPCACHE_APPCACHE_SERVICE_H_

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/appcache/appcache_storage.h"

class URLRequestContext;

namespace appcache {

class AppCacheBackendImpl;

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile. Each instance has
// exclusive access to it's cache_directory on disk.
class AppCacheService {
 public:
  AppCacheService();
  virtual ~AppCacheService();

  void Initialize(const FilePath& cache_directory);

  void PurgeMemory() {
    if (storage_.get())
      storage_->PurgeMemory();
  }

  // Context for use during cache updates, should only be accessed
  // on the IO thread. We do NOT add a reference to the request context,
  // it is the callers responsibility to ensure that the pointer
  // remains valid while set.
  URLRequestContext* request_context() const { return request_context_; }
  void set_request_context(URLRequestContext* context) {
    request_context_ = context;
  }

  // Track which processes are using this appcache service.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) const {
    BackendMap::const_iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  AppCacheStorage* storage() const { return storage_.get(); }

 protected:
  // Deals with persistence.
  scoped_ptr<AppCacheStorage> storage_;

  // Track current processes.  One 'backend' per child process.
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;
  BackendMap backends_;

  // Context for use during cache updates.
  URLRequestContext* request_context_;

  // TODO(jennb): service state: e.g. reached quota?

  DISALLOW_COPY_AND_ASSIGN(AppCacheService);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
