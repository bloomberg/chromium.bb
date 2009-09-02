// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_service.h"

using appcache::AppCache;
using appcache::AppCacheHost;
using appcache::AppCacheGroup;
using appcache::AppCacheService;

namespace {

class AppCacheGroupTest : public testing::Test {
};

}  // namespace

TEST(AppCacheGroupTest, AddRemoveCache) {
  AppCacheService service;
  scoped_refptr<AppCacheGroup> group =
    new AppCacheGroup(&service, GURL::EmptyGURL());

  base::TimeTicks ticks = base::TimeTicks::Now();

  AppCache* cache1 = new AppCache(&service, 111);
  cache1->set_complete(true);
  cache1->set_update_time(ticks);
  cache1->set_owning_group(group);
  group->AddCache(cache1);
  EXPECT_EQ(cache1, group->newest_complete_cache());

  // Adding older cache does not change newest complete cache.
  AppCache* cache2 = new AppCache(&service, 222);
  cache2->set_complete(true);
  cache2->set_update_time(ticks - base::TimeDelta::FromDays(1));
  cache2->set_owning_group(group);
  group->AddCache(cache2);
  EXPECT_EQ(cache1, group->newest_complete_cache());

  // Adding newer cache does change newest complete cache.
  AppCache* cache3 = new AppCache(&service, 333);
  cache3->set_complete(true);
  cache3->set_update_time(ticks + base::TimeDelta::FromDays(1));
  cache3->set_owning_group(group);
  group->AddCache(cache3);
  EXPECT_EQ(cache3, group->newest_complete_cache());

  // Old caches can always be removed.
  EXPECT_TRUE(group->RemoveCache(cache1));
  EXPECT_EQ(cache3, group->newest_complete_cache());  // newest unchanged

  // Cannot remove newest cache if there are older caches.
  EXPECT_FALSE(group->RemoveCache(cache3));
  EXPECT_EQ(cache3, group->newest_complete_cache());  // newest unchanged

  // Can remove newest cache after all older caches are removed.
  EXPECT_TRUE(group->RemoveCache(cache2));
  EXPECT_EQ(cache3, group->newest_complete_cache());  // newest unchanged
  EXPECT_TRUE(group->RemoveCache(cache3));
}

TEST(AppCacheGroupTest, CleanupUnusedGroup) {
  AppCacheService service;
  AppCacheGroup* group = new AppCacheGroup(&service, GURL::EmptyGURL());

  AppCacheHost host1(1, NULL);
  AppCacheHost host2(2, NULL);

  base::TimeTicks ticks = base::TimeTicks::Now();

  AppCache* cache1 = new AppCache(&service, 111);
  cache1->set_complete(true);
  cache1->set_update_time(ticks);
  cache1->set_owning_group(group);
  group->AddCache(cache1);
  EXPECT_EQ(cache1, group->newest_complete_cache());

  host1.set_selected_cache(cache1);
  host2.set_selected_cache(cache1);

  AppCache* cache2 = new AppCache(&service, 222);
  cache2->set_complete(true);
  cache2->set_update_time(ticks + base::TimeDelta::FromDays(1));
  cache2->set_owning_group(group);
  group->AddCache(cache2);
  EXPECT_EQ(cache2, group->newest_complete_cache());

  // Unassociate all hosts from older cache.
  host1.set_selected_cache(NULL);
  host2.set_selected_cache(NULL);
}
