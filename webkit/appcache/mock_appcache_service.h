// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
  MockAppCacheService() : AppCacheService(NULL) {
    storage_.reset(new MockAppCacheStorage(this));
  }

  void set_quota_manager_proxy(quota::QuotaManagerProxy* proxy) {
    quota_manager_proxy_ = proxy;
  }
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_SERVICE_H_
