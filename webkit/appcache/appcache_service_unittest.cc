// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_service.h"

using appcache::AppCache;
using appcache::AppCacheGroup;
using appcache::AppCacheService;

namespace {

class AppCacheServiceTest : public testing::Test {
};

}  // namespace

TEST(AppCacheServiceTest, AddRemoveCache) {
  AppCacheService service;
  scoped_refptr<AppCache> cache = new AppCache(&service, 111);
  service.RemoveCache(cache);

  // Removing non-existing cache from service should not fail.
  AppCacheService dummy;
  dummy.RemoveCache(cache);
}

TEST(AppCacheServiceTest, AddRemoveGroup) {
  AppCacheService service;
  scoped_refptr<AppCacheGroup> group =
    new AppCacheGroup(&service, GURL::EmptyGURL());
  service.RemoveGroup(group);

  // Removing non-existing group from service should not fail.
  AppCacheService dummy;
  dummy.RemoveGroup(group);
}
