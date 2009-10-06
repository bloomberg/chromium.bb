// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_update_job.h"

namespace {

class TestAppCacheFrontend : public appcache::AppCacheFrontend {
 public:
  TestAppCacheFrontend()
      : last_host_id_(-1), last_cache_id_(-1),
        last_status_(appcache::OBSOLETE) {
  }

  virtual void OnCacheSelected(int host_id, int64 cache_id ,
                               appcache::Status status) {
    last_host_id_ = host_id;
    last_cache_id_ = cache_id;
    last_status_ = status;
  }

  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               appcache::Status status) {
  }

  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             appcache::EventID event_id) {
  }

  int last_host_id_;
  int64 last_cache_id_;
  appcache::Status last_status_;
};

}  // namespace anon

namespace appcache {

class AppCacheGroupTest : public testing::Test {
};

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
  TestAppCacheFrontend frontend;
  AppCacheGroup* group = new AppCacheGroup(&service, GURL::EmptyGURL());

  AppCacheHost host1(1, &frontend, &service);
  AppCacheHost host2(2, &frontend, &service);

  base::TimeTicks ticks = base::TimeTicks::Now();

  AppCache* cache1 = new AppCache(&service, 111);
  cache1->set_complete(true);
  cache1->set_update_time(ticks);
  cache1->set_owning_group(group);
  group->AddCache(cache1);
  EXPECT_EQ(cache1, group->newest_complete_cache());

  host1.AssociateCache(cache1);
  EXPECT_EQ(frontend.last_host_id_, host1.host_id());
  EXPECT_EQ(frontend.last_cache_id_, cache1->cache_id());
  EXPECT_EQ(frontend.last_status_, appcache::IDLE);

  host2.AssociateCache(cache1);
  EXPECT_EQ(frontend.last_host_id_, host2.host_id());
  EXPECT_EQ(frontend.last_cache_id_, cache1->cache_id());
  EXPECT_EQ(frontend.last_status_, appcache::IDLE);

  AppCache* cache2 = new AppCache(&service, 222);
  cache2->set_complete(true);
  cache2->set_update_time(ticks + base::TimeDelta::FromDays(1));
  cache2->set_owning_group(group);
  group->AddCache(cache2);
  EXPECT_EQ(cache2, group->newest_complete_cache());

  // Unassociate all hosts from older cache.
  host1.AssociateCache(NULL);
  host2.AssociateCache(NULL);
  EXPECT_EQ(frontend.last_host_id_, host2.host_id());
  EXPECT_EQ(frontend.last_cache_id_, appcache::kNoCacheId);
  EXPECT_EQ(frontend.last_status_, appcache::UNCACHED);
}

TEST(AppCacheGroupTest, StartUpdate) {
  /* TODO(jennb) - uncomment after AppCacheGroup::StartUpdate does something.
  AppCacheService service;
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, GURL("http://foo.com"));

  // Set state to checking to prevent update job from executing fetches.
  group->update_status_ = AppCacheGroup::CHECKING;
  group->StartUpdate();
  AppCacheUpdateJob* update = group->update_job_;
  EXPECT_TRUE(update != NULL);

  // Start another update, check that same update job is in use.
  group->StartUpdateWithHost(NULL);
  EXPECT_EQ(update, group->update_job_);

  // Remove update job's reference to this group.
  delete update;
  EXPECT_TRUE(group->update_job_ == NULL);
  EXPECT_EQ(AppCacheGroup::IDLE, group->update_status());
  */
}

}  // namespace appcache
