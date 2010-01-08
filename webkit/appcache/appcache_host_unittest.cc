// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class AppCacheHostTest : public testing::Test {
 public:
  AppCacheHostTest() {
    get_status_callback_.reset(
        NewCallback(this, &AppCacheHostTest::GetStatusCallback));
    start_update_callback_.reset(
        NewCallback(this, &AppCacheHostTest::StartUpdateCallback));
    swap_cache_callback_.reset(
        NewCallback(this, &AppCacheHostTest::SwapCacheCallback));
  }

  class MockFrontend : public AppCacheFrontend {
   public:
    MockFrontend()
        : last_host_id_(-222), last_cache_id_(-222),
          last_status_(appcache::OBSOLETE),
          last_status_changed_(appcache::OBSOLETE),
          last_event_id_(appcache::OBSOLETE_EVENT) {
    }

    virtual void OnCacheSelected(int host_id, int64 cache_id ,
                                 appcache::Status status) {
      last_host_id_ = host_id;
      last_cache_id_ = cache_id;
      last_status_ = status;
    }

    virtual void OnStatusChanged(const std::vector<int>& host_ids,
                                 appcache::Status status) {
      last_status_changed_ = status;
    }

    virtual void OnEventRaised(const std::vector<int>& host_ids,
                               appcache::EventID event_id) {
      last_event_id_ = event_id;
    }

    int last_host_id_;
    int64 last_cache_id_;
    appcache::Status last_status_;
    appcache::Status last_status_changed_;
    appcache::EventID last_event_id_;
  };

  void GetStatusCallback(Status status, void* param) {
    last_status_result_ = status;
    last_callback_param_ = param;
  }

  void StartUpdateCallback(bool result, void* param) {
    last_start_result_ = result;
    last_callback_param_ = param;
  }

  void SwapCacheCallback(bool result, void* param) {
    last_swap_result_ = result;
    last_callback_param_ = param;
  }

  // Mock classes for the 'host' to work with
  MockAppCacheService service_;
  MockFrontend mock_frontend_;

  // Mock callbacks we expect to receive from the 'host'
  scoped_ptr<appcache::GetStatusCallback> get_status_callback_;
  scoped_ptr<appcache::StartUpdateCallback> start_update_callback_;
  scoped_ptr<appcache::SwapCacheCallback> swap_cache_callback_;

  Status last_status_result_;
  bool last_swap_result_;
  bool last_start_result_;
  void* last_callback_param_;
};

TEST_F(AppCacheHostTest, Basic) {
  // Construct a host and test what state it appears to be in.
  AppCacheHost host(1, &mock_frontend_, &service_);
  EXPECT_EQ(1, host.host_id());
  EXPECT_EQ(&service_, host.service());
  EXPECT_EQ(&mock_frontend_, host.frontend());
  EXPECT_EQ(NULL, host.associated_cache());
  EXPECT_FALSE(host.is_selection_pending());

  // See that the callbacks are delivered immediately
  // and respond as if there is no cache selected.
  last_status_result_ = OBSOLETE;
  host.GetStatusWithCallback(get_status_callback_.get(),
                             reinterpret_cast<void*>(1));
  EXPECT_EQ(UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);

  last_start_result_ = true;
  host.StartUpdateWithCallback(start_update_callback_.get(),
                               reinterpret_cast<void*>(2));
  EXPECT_FALSE(last_start_result_);
  EXPECT_EQ(reinterpret_cast<void*>(2), last_callback_param_);

  last_swap_result_ = true;
  host.SwapCacheWithCallback(swap_cache_callback_.get(),
                             reinterpret_cast<void*>(3));
  EXPECT_FALSE(last_swap_result_);
  EXPECT_EQ(reinterpret_cast<void*>(3), last_callback_param_);
}

TEST_F(AppCacheHostTest, SelectNoCache) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = OBSOLETE;

  AppCacheHost host(1, &mock_frontend_, &service_);
  host.SelectCache(GURL("http://whatever/"), kNoCacheId, GURL());

  // We should have received an OnCacheSelected msg
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(UNCACHED, mock_frontend_.last_status_);

  // Otherwise, see that it respond as if there is no cache selected.
  EXPECT_EQ(1, host.host_id());
  EXPECT_EQ(&service_, host.service());
  EXPECT_EQ(&mock_frontend_, host.frontend());
  EXPECT_EQ(NULL, host.associated_cache());
  EXPECT_FALSE(host.is_selection_pending());
}

TEST_F(AppCacheHostTest, ForeignEntry) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = OBSOLETE;

  AppCacheHost host(1, &mock_frontend_, &service_);
  host.MarkAsForeignEntry(GURL("http://whatever/"), 22);

  // We should have received an OnCacheSelected msg for kNoCacheId.
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(UNCACHED, mock_frontend_.last_status_);

  // See that it respond as if there is no cache selected.
  EXPECT_EQ(1, host.host_id());
  EXPECT_EQ(&service_, host.service());
  EXPECT_EQ(&mock_frontend_, host.frontend());
  EXPECT_EQ(NULL, host.associated_cache());
  EXPECT_FALSE(host.is_selection_pending());
}

TEST_F(AppCacheHostTest, FailedCacheLoad) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = OBSOLETE;

  AppCacheHost host(1, &mock_frontend_, &service_);
  EXPECT_FALSE(host.is_selection_pending());

  const int kMockCacheId = 333;

  // Put it in a state where we're waiting on a cache
  // load prior to finishing cache selection.
  host.pending_selected_cache_id_ = kMockCacheId;
  EXPECT_TRUE(host.is_selection_pending());

  // The callback should not occur until we finish cache selection.
  last_status_result_ = OBSOLETE;
  last_callback_param_ = reinterpret_cast<void*>(-1);
  host.GetStatusWithCallback(get_status_callback_.get(),
                             reinterpret_cast<void*>(1));
  EXPECT_EQ(OBSOLETE, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(-1), last_callback_param_);

  // Satisfy the load with NULL, a failure.
  host.OnCacheLoaded(NULL, kMockCacheId);

  // Cache selection should have finished
  EXPECT_FALSE(host.is_selection_pending());
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(UNCACHED, mock_frontend_.last_status_);

  // Callback should have fired upon completing the cache load too.
  EXPECT_EQ(UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);
}

TEST_F(AppCacheHostTest, FailedGroupLoad) {
  AppCacheHost host(1, &mock_frontend_, &service_);

  const GURL kMockManifestUrl("http://foo.bar/baz");

  // Put it in a state where we're waiting on a cache
  // load prior to finishing cache selection.
  host.pending_selected_manifest_url_ = kMockManifestUrl;
  EXPECT_TRUE(host.is_selection_pending());

  // The callback should not occur until we finish cache selection.
  last_status_result_ = OBSOLETE;
  last_callback_param_ = reinterpret_cast<void*>(-1);
  host.GetStatusWithCallback(get_status_callback_.get(),
                             reinterpret_cast<void*>(1));
  EXPECT_EQ(OBSOLETE, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(-1), last_callback_param_);

  // Satisfy the load will NULL, a failure.
  host.OnGroupLoaded(NULL, kMockManifestUrl);

  // Cache selection should have finished
  EXPECT_FALSE(host.is_selection_pending());
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(UNCACHED, mock_frontend_.last_status_);

  // Callback should have fired upon completing the group load.
  EXPECT_EQ(UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);
}

TEST_F(AppCacheHostTest, SetSwappableCache) {
  AppCacheHost host(1, &mock_frontend_, &service_);
  host.SetSwappableCache(NULL);
  EXPECT_FALSE(host.swappable_cache_.get());

  scoped_refptr<AppCacheGroup> group1 =
      new AppCacheGroup(&service_, GURL(), service_.storage()->NewGroupId());
  host.SetSwappableCache(group1);
  EXPECT_FALSE(host.swappable_cache_.get());

  AppCache* cache1 = new AppCache(&service_, 111);
  cache1->set_complete(true);
  group1->AddCache(cache1);
  host.SetSwappableCache(group1);
  EXPECT_EQ(cache1, host.swappable_cache_.get());

  mock_frontend_.last_host_id_ = -222;  // to verify we received OnCacheSelected

  host.AssociateCache(cache1);
  EXPECT_FALSE(host.swappable_cache_.get());  // was same as associated cache
  EXPECT_EQ(appcache::IDLE, host.GetStatus());
  // verify OnCacheSelected was called
  EXPECT_EQ(host.host_id(), mock_frontend_.last_host_id_);
  EXPECT_EQ(cache1->cache_id(), mock_frontend_.last_cache_id_);
  EXPECT_EQ(appcache::IDLE, mock_frontend_.last_status_);

  AppCache* cache2 = new AppCache(&service_, 222);
  cache2->set_complete(true);
  group1->AddCache(cache2);
  EXPECT_EQ(cache2, host.swappable_cache_.get());  // updated to newest

  scoped_refptr<AppCacheGroup> group2 =
      new AppCacheGroup(&service_, GURL("http://foo.com"),
                        service_.storage()->NewGroupId());
  AppCache* cache3 = new AppCache(&service_, 333);
  cache3->set_complete(true);
  group2->AddCache(cache3);

  AppCache* cache4 = new AppCache(&service_, 444);
  cache4->set_complete(true);
  group2->AddCache(cache4);
  EXPECT_EQ(cache2, host.swappable_cache_.get());  // unchanged

  host.AssociateCache(cache3);
  EXPECT_EQ(cache4, host.swappable_cache_.get());  // newest cache in group2
  EXPECT_FALSE(group1->HasCache());  // both caches in group1 have refcount 0

  host.AssociateCache(NULL);
  EXPECT_FALSE(host.swappable_cache_.get());
  EXPECT_FALSE(group2->HasCache());  // both caches in group2 have refcount 0

  // Host adds reference to newest cache when an update is complete.
  AppCache* cache5 = new AppCache(&service_, 555);
  cache5->set_complete(true);
  group2->AddCache(cache5);
  host.group_being_updated_ = group2;
  host.OnUpdateComplete(group2);
  EXPECT_FALSE(host.group_being_updated_);
  EXPECT_EQ(cache5, host.swappable_cache_.get());

  group2->RemoveCache(cache5);
  EXPECT_FALSE(group2->HasCache());
  host.group_being_updated_ = group2;
  host.OnUpdateComplete(group2);
  EXPECT_FALSE(host.group_being_updated_);
  EXPECT_FALSE(host.swappable_cache_.get());  // group2 had no newest cache
}

}  // namespace appcache

