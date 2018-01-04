// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;
using storage::QuotaClient;
using storage::QuotaClientList;
using storage::SpecialStoragePolicy;
using storage::UsageTracker;

namespace content {

namespace {

void DidGetGlobalUsage(bool* done,
                       int64_t* usage_out,
                       int64_t* unlimited_usage_out,
                       int64_t usage,
                       int64_t unlimited_usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
  *unlimited_usage_out = unlimited_usage;
}

void DidGetUsage(bool* done, int64_t* usage_out, int64_t usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
}

void DidGetUsageBreakdown(
    bool* done,
    int64_t* usage_out,
    base::flat_map<QuotaClient::ID, int64_t>* usage_breakdown_out,
    int64_t usage,
    base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
  *usage_breakdown_out = usage_breakdown;
}

}  // namespace

class MockQuotaClient : public QuotaClient {
 public:
  MockQuotaClient() = default;
  ~MockQuotaClient() override = default;

  ID id() const override { return kFileSystem; }

  void OnQuotaManagerDestroyed() override {}

  void GetOriginUsage(const GURL& origin,
                      StorageType type,
                      const GetUsageCallback& callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    int64_t usage = GetUsage(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, usage));
  }

  void GetOriginsForType(StorageType type,
                         const GetOriginsCallback& callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<GURL> origins;
    for (UsageMap::const_iterator itr = usage_map_.begin();
         itr != usage_map_.end(); ++itr) {
      origins.insert(itr->first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, origins));
  }

  void GetOriginsForHost(StorageType type,
                         const std::string& host,
                         const GetOriginsCallback& callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<GURL> origins;
    for (UsageMap::const_iterator itr = usage_map_.begin();
         itr != usage_map_.end(); ++itr) {
      if (net::GetHostOrSpecFromURL(itr->first) == host)
        origins.insert(itr->first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, origins));
  }

  void DeleteOriginData(const GURL& origin,
                        StorageType type,
                        const DeletionCallback& callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    usage_map_.erase(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, QuotaStatusCode::kOk));
  }

  bool DoesSupport(StorageType type) const override {
    return type == StorageType::kTemporary;
  }

  int64_t GetUsage(const GURL& origin) {
    UsageMap::const_iterator found = usage_map_.find(origin);
    if (found == usage_map_.end())
      return 0;
    return found->second;
  }

  void SetUsage(const GURL& origin, int64_t usage) {
    usage_map_[origin] = usage;
  }

  int64_t UpdateUsage(const GURL& origin, int64_t delta) {
    return usage_map_[origin] += delta;
  }

 private:
  typedef std::map<GURL, int64_t> UsageMap;

  UsageMap usage_map_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaClient);
};

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(new MockSpecialStoragePolicy()),
        usage_tracker_(GetUsageTrackerList(),
                       StorageType::kTemporary,
                       storage_policy_.get(),
                       NULL) {}

  ~UsageTrackerTest() override = default;

  UsageTracker* usage_tracker() {
    return &usage_tracker_;
  }

  void UpdateUsage(const GURL& origin, int64_t delta) {
    quota_client_.UpdateUsage(origin, delta);
    usage_tracker_.UpdateUsageCache(quota_client_.id(), origin, delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const GURL& origin, int64_t delta) {
    quota_client_.UpdateUsage(origin, delta);
  }

  void GetGlobalLimitedUsage(int64_t* limited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalLimitedUsage(base::Bind(
        &DidGetUsage, &done, limited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalUsage(base::Bind(
        &DidGetGlobalUsage,
        &done, usage, unlimited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GetHostUsage(const std::string& host, int64_t* usage) {
    bool done = false;
    usage_tracker_.GetHostUsage(host, base::Bind(&DidGetUsage, &done, usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GetHostUsageBreakdown(
      const std::string& host,
      int64_t* usage,
      base::flat_map<QuotaClient::ID, int64_t>* usage_breakdown) {
    bool done = false;
    usage_tracker_.GetHostUsageWithBreakdown(
        host, base::Bind(&DidGetUsageBreakdown, &done, usage, usage_breakdown));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GrantUnlimitedStoragePolicy(const GURL& origin) {
    if (!storage_policy_->IsStorageUnlimited(origin)) {
      storage_policy_->AddUnlimited(origin);
      storage_policy_->NotifyGranted(
          origin, SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void RevokeUnlimitedStoragePolicy(const GURL& origin) {
    if (storage_policy_->IsStorageUnlimited(origin)) {
      storage_policy_->RemoveUnlimited(origin);
      storage_policy_->NotifyRevoked(
          origin, SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void SetUsageCacheEnabled(const GURL& origin, bool enabled) {
    usage_tracker_.SetUsageCacheEnabled(
        quota_client_.id(), origin, enabled);
  }

 private:
  QuotaClientList GetUsageTrackerList() {
    QuotaClientList client_list;
    client_list.push_back(&quota_client_);
    return client_list;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  MockQuotaClient quota_client_;
  UsageTracker usage_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTest);
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  int64_t host_usage = 0;
  base::flat_map<QuotaClient::ID, int64_t> host_usage_breakdown;
  base::flat_map<QuotaClient::ID, int64_t> host_usage_breakdown_expected;
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  const GURL origin("http://example.com");
  const std::string host(net::GetHostOrSpecFromURL(origin));

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown_expected[QuotaClient::kFileSystem] = 100;
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  GrantUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  int64_t host_usage = 0;
  base::flat_map<QuotaClient::ID, int64_t> host_usage_breakdown;
  base::flat_map<QuotaClient::ID, int64_t> host_usage_breakdown_expected;

  const GURL origin("http://example.com");
  const std::string host(net::GetHostOrSpecFromURL(origin));

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown_expected[QuotaClient::kFileSystem] = 100;
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  UpdateUsageWithoutNotification(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  GrantUnlimitedStoragePolicy(origin);
  UpdateUsageWithoutNotification(origin, 100);
  SetUsageCacheEnabled(origin, false);
  UpdateUsageWithoutNotification(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  EXPECT_EQ(400, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  host_usage_breakdown_expected[QuotaClient::kFileSystem] = 400;
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(400, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);

  SetUsageCacheEnabled(origin, true);
  UpdateUsage(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(500, host_usage);
  GetHostUsageBreakdown(host, &host_usage, &host_usage_breakdown);
  host_usage_breakdown_expected[QuotaClient::kFileSystem] = 500;
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown);
}

TEST_F(UsageTrackerTest, LimitedGlobalUsageTest) {
  const GURL kNormal("http://normal");
  const GURL kUnlimited("http://unlimited");
  const GURL kNonCached("http://non_cached");
  const GURL kNonCachedUnlimited("http://non_cached-unlimited");

  GrantUnlimitedStoragePolicy(kUnlimited);
  GrantUnlimitedStoragePolicy(kNonCachedUnlimited);

  SetUsageCacheEnabled(kNonCached, false);
  SetUsageCacheEnabled(kNonCachedUnlimited, false);

  UpdateUsageWithoutNotification(kNormal, 1);
  UpdateUsageWithoutNotification(kUnlimited, 2);
  UpdateUsageWithoutNotification(kNonCached, 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 8);

  int64_t limited_usage = 0;
  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;

  GetGlobalLimitedUsage(&limited_usage);
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 4, limited_usage);
  EXPECT_EQ(1 + 2 + 4 + 8, total_usage);
  EXPECT_EQ(2 + 8, unlimited_usage);

  UpdateUsageWithoutNotification(kNonCached, 16 - 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 32 - 8);

  GetGlobalLimitedUsage(&limited_usage);
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 16, limited_usage);
  EXPECT_EQ(1 + 2 + 16 + 32, total_usage);
  EXPECT_EQ(2 + 32, unlimited_usage);
}


}  // namespace content
