// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_frontend_impl.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class AppCacheTest : public testing::Test {
};

TEST(AppCacheTest, CleanupUnusedCache) {
  MockAppCacheService service;
  AppCacheFrontendImpl frontend;
  scoped_refptr<AppCache> cache(new AppCache(&service, 111));
  cache->set_complete(true);
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, GURL("http://blah/manifest")));
  group->AddCache(cache);

  AppCacheHost host1(1, &frontend, &service);
  AppCacheHost host2(2, &frontend, &service);

  host1.AssociateCache(cache.get());
  host2.AssociateCache(cache.get());

  host1.AssociateCache(NULL);
  host2.AssociateCache(NULL);
}

TEST(AppCacheTest, AddModifyEntry) {
  MockAppCacheService service;
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

TEST(AppCacheTest, InitializeWithManifest) {
  MockAppCacheService service;

  scoped_refptr<AppCache> cache = new AppCache(&service, 1234);
  EXPECT_TRUE(cache->fallback_namespaces_.empty());
  EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
  EXPECT_FALSE(cache->online_whitelist_all_);

  Manifest manifest;
  manifest.explicit_urls.insert("http://one.com");
  manifest.explicit_urls.insert("http://two.com");
  manifest.fallback_namespaces.push_back(
      FallbackNamespace(GURL("http://fb1.com"), GURL("http://fbone.com")));
  manifest.online_whitelist_namespaces.push_back(GURL("http://w1.com"));
  manifest.online_whitelist_namespaces.push_back(GURL("http://w2.com"));
  manifest.online_whitelist_all = true;

  cache->InitializeWithManifest(&manifest);
  const std::vector<FallbackNamespace>& fallbacks =
      cache->fallback_namespaces_;
  size_t expected = 1;
  EXPECT_EQ(expected, fallbacks.size());
  EXPECT_EQ(GURL("http://fb1.com"), fallbacks[0].first);
  EXPECT_EQ(GURL("http://fbone.com"), fallbacks[0].second);
  const std::vector<GURL>& whitelist = cache->online_whitelist_namespaces_;
  expected = 2;
  EXPECT_EQ(expected, whitelist.size());
  EXPECT_EQ(GURL("http://w1.com"), whitelist[0]);
  EXPECT_EQ(GURL("http://w2.com"), whitelist[1]);
  EXPECT_TRUE(cache->online_whitelist_all_);

  // Ensure collections in manifest were taken over by the cache rather than
  // copied.
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
}

}  // namespace appacache

