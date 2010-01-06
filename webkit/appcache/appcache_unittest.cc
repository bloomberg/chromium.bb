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
      new AppCacheGroup(&service, GURL("http://blah/manifest"), 111));
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
  EXPECT_TRUE(cache->AddOrModifyEntry(kUrl2, entry2));
  EXPECT_EQ(entry2.types(), cache->GetEntry(kUrl2)->types());

  // Expected to return false when an existing entry is modified.
  AppCacheEntry entry3(AppCacheEntry::EXPLICIT);
  EXPECT_FALSE(cache->AddOrModifyEntry(kUrl1, entry3));
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

TEST(AppCacheTest, FindResponseForRequest) {
  MockAppCacheService service;

  const GURL kOnlineNamespaceUrl("http://blah/online_namespace");
  const GURL kFallbackEntryUrl1("http://blah/fallback_entry1");
  const GURL kFallbackNamespaceUrl1("http://blah/fallback_namespace/");
  const GURL kFallbackEntryUrl2("http://blah/fallback_entry2");
  const GURL kFallbackNamespaceUrl2("http://blah/fallback_namespace/longer");
  const GURL kManifestUrl("http://blah/manifest");
  const GURL kForeignExplicitEntryUrl("http://blah/foreign");
  const GURL kInOnlineNamespaceUrl(
      "http://blah/online_namespace/network");
  const GURL kExplicitInOnlineNamespaceUrl(
      "http://blah/online_namespace/explicit");
  const GURL kFallbackTestUrl1("http://blah/fallback_namespace/1");
  const GURL kFallbackTestUrl2("http://blah/fallback_namespace/longer2");

  const int64 kFallbackResponseId1 = 1;
  const int64 kFallbackResponseId2 = 2;
  const int64 kManifestResponseId = 3;
  const int64 kForeignExplicitResponseId = 4;
  const int64 kExplicitInOnlineNamespaceResponseId = 5;

  Manifest manifest;
  manifest.online_whitelist_namespaces.push_back(kOnlineNamespaceUrl);
  manifest.fallback_namespaces.push_back(
      FallbackNamespace(kFallbackNamespaceUrl1, kFallbackEntryUrl1));
  manifest.fallback_namespaces.push_back(
      FallbackNamespace(kFallbackNamespaceUrl2, kFallbackEntryUrl2));

  // Create a cache with some namespaces and entries.
  scoped_refptr<AppCache> cache = new AppCache(&service, 1234);
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kFallbackEntryUrl1,
      AppCacheEntry(AppCacheEntry::FALLBACK, kFallbackResponseId1));
  cache->AddEntry(
      kFallbackEntryUrl2,
      AppCacheEntry(AppCacheEntry::FALLBACK, kFallbackResponseId2));
  cache->AddEntry(
      kManifestUrl,
      AppCacheEntry(AppCacheEntry::MANIFEST, kManifestResponseId));
  cache->AddEntry(
      kForeignExplicitEntryUrl,
      AppCacheEntry(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
                    kForeignExplicitResponseId));
  cache->AddEntry(
      kExplicitInOnlineNamespaceUrl,
      AppCacheEntry(AppCacheEntry::EXPLICIT,
                    kExplicitInOnlineNamespaceResponseId));
  cache->set_complete(true);

  // See that we get expected results from FindResponseForRequest

  bool found = false;
  AppCacheEntry entry;
  AppCacheEntry fallback_entry;
  GURL fallback_namespace;
  bool network_namespace = false;

  found = cache->FindResponseForRequest(GURL("http://blah/miss"),
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(kForeignExplicitEntryUrl,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kForeignExplicitResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kManifestUrl,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kManifestResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kInOnlineNamespaceUrl,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(network_namespace);

  network_namespace = false;  // reset

  found = cache->FindResponseForRequest(kExplicitInOnlineNamespaceUrl,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kExplicitInOnlineNamespaceResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kFallbackTestUrl1,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId1, fallback_entry.response_id());
  EXPECT_FALSE(network_namespace);

  fallback_entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kFallbackTestUrl2,
      &entry, &fallback_entry, &fallback_namespace, &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId2, fallback_entry.response_id());
  EXPECT_FALSE(network_namespace);
}

}  // namespace appacache

