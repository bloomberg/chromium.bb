// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class AppCacheStorageTest : public testing::Test {
};

TEST(AppCacheStorageTest, AddRemoveCache) {
  MockAppCacheService service;
  scoped_refptr<AppCache> cache = new AppCache(&service, 111);
  service.storage()->working_set()->RemoveCache(cache);

  // Removing non-existing cache from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveCache(cache);
}

TEST(AppCacheStorageTest, AddRemoveGroup) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, GURL::EmptyGURL());
  service.storage()->working_set()->RemoveGroup(group);

  // Removing non-existing group from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveGroup(group);
}

}  // namespace appcache
