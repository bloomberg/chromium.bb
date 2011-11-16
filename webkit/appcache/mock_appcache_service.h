// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_SERVICE_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_SERVICE_H_

#include "base/compiler_specific.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/mock_appcache_storage.h"
#include "webkit/quota/quota_manager.h"

namespace appcache {

// For use by unit tests.
class MockAppCacheService : public AppCacheService {
 public:
  MockAppCacheService()
    : AppCacheService(NULL),
      mock_delete_appcaches_for_origin_result_(net::OK),
      delete_called_count_(0) {
    storage_.reset(new MockAppCacheStorage(this));
  }

  // Just returns a canned completion code without actually
  // removing groups and caches in our mock storage instance.
  virtual void DeleteAppCachesForOrigin(
      const GURL& origin,
      net::OldCompletionCallback* callback) OVERRIDE;

  void set_quota_manager_proxy(quota::QuotaManagerProxy* proxy) {
    quota_manager_proxy_ = proxy;
  }

  void set_mock_delete_appcaches_for_origin_result(int rv) {
    mock_delete_appcaches_for_origin_result_ = rv;
  }

  int delete_called_count() const { return delete_called_count_; }
 private:
  int mock_delete_appcaches_for_origin_result_;
  int delete_called_count_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_SERVICE_H_
