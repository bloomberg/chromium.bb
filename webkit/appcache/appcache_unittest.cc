// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_frontend_impl.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

class AppCacheTest : public testing::Test {
};

TEST(AppCacheTest, CleanupUnusedCache) {
  AppCacheService service;
  AppCacheFrontendImpl frontend;
  AppCache* cache = new AppCache(&service, 111);
  cache->set_complete(true);

  AppCacheHost host1(1, &frontend, &service);
  AppCacheHost host2(2, &frontend, &service);

  host1.AssociateCache(cache);
  host2.AssociateCache(cache);

  host1.AssociateCache(NULL);
  host2.AssociateCache(NULL);
}

TEST(AppCacheTest, AddModifyEntry) {
  AppCacheService service;
  scoped_refptr<AppCache> cache = new AppCache(&service, 111);

  const GURL kUrl1("http://foo.com");
  AppCacheEntry entry1(AppCacheEntry::MASTER);
  cache->AddEntry(kUrl1, entry1);
  EXPECT_EQ(entry1.types(), cache->GetEntry(kUrl1)->types());

  const GURL kUrl2("http://bar.com");
  AppCacheEntry entry2(AppCacheEntry::FALLBACK);
  cache->AddOrModifyEntry(kUrl2, entry2);
  EXPECT_EQ(entry2.types(), cache->GetEntry(kUrl2)->types());

  AppCacheEntry entry3(AppCacheEntry::EXPLICIT);
  cache->AddOrModifyEntry(kUrl1, entry3);
  EXPECT_EQ((AppCacheEntry::MASTER | AppCacheEntry::EXPLICIT),
    cache->GetEntry(kUrl1)->types());
  EXPECT_EQ(entry2.types(), cache->GetEntry(kUrl2)->types());  // unchanged
}

}  // namespace appacache

