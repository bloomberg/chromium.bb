// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;

namespace storage {

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

class UsageTrackerTestQuotaClient : public QuotaClient {
 public:
  UsageTrackerTestQuotaClient() = default;

  QuotaClientType type() const override { return QuotaClientType::kFileSystem; }

  void OnQuotaManagerDestroyed() override {}

  void GetOriginUsage(const url::Origin& origin,
                      StorageType type,
                      GetUsageCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    int64_t usage = GetUsage(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), usage));
  }

  void GetOriginsForType(StorageType type,
                         GetOriginsCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<url::Origin> origins;
    for (const auto& origin_usage_pair : origin_usage_map_)
      origins.insert(origin_usage_pair.first);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), origins));
  }

  void GetOriginsForHost(StorageType type,
                         const std::string& host,
                         GetOriginsCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<url::Origin> origins;
    for (const auto& origin_usage_pair : origin_usage_map_) {
      if (net::GetHostOrSpecFromURL(origin_usage_pair.first.GetURL()) == host)
        origins.insert(origin_usage_pair.first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), origins));
  }

  void DeleteOriginData(const url::Origin& origin,
                        StorageType type,
                        DeletionCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    origin_usage_map_.erase(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), QuotaStatusCode::kOk));
  }

  void PerformStorageCleanup(blink::mojom::StorageType type,
                             base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  bool DoesSupport(StorageType type) const override {
    return type == StorageType::kTemporary;
  }

  int64_t GetUsage(const url::Origin& origin) {
    auto found = origin_usage_map_.find(origin);
    if (found == origin_usage_map_.end())
      return 0;
    return found->second;
  }

  void SetUsage(const url::Origin& origin, int64_t usage) {
    origin_usage_map_[origin] = usage;
  }

  int64_t UpdateUsage(const url::Origin& origin, int64_t delta) {
    return origin_usage_map_[origin] += delta;
  }

 private:
  ~UsageTrackerTestQuotaClient() override = default;

  std::map<url::Origin, int64_t> origin_usage_map_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTestQuotaClient);
};

}  // namespace

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(new MockSpecialStoragePolicy()),
        quota_client_(base::MakeRefCounted<UsageTrackerTestQuotaClient>()),
        usage_tracker_(GetUsageTrackerList(),
                       StorageType::kTemporary,
                       storage_policy_.get()) {}

  ~UsageTrackerTest() override = default;

  UsageTracker* usage_tracker() {
    return &usage_tracker_;
  }

  static void DidGetUsageBreakdown(
      bool* done,
      int64_t* usage_out,
      blink::mojom::UsageBreakdownPtr* usage_breakdown_out,
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    EXPECT_FALSE(*done);
    *usage_out = usage;
    *usage_breakdown_out = std::move(usage_breakdown);
    *done = true;
  }

  void UpdateUsage(const url::Origin& origin, int64_t delta) {
    quota_client_->UpdateUsage(origin, delta);
    usage_tracker_.UpdateUsageCache(quota_client_->type(), origin, delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const url::Origin& origin,
                                      int64_t delta) {
    quota_client_->UpdateUsage(origin, delta);
  }

  void GetGlobalLimitedUsage(int64_t* limited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalLimitedUsage(
        base::BindOnce(&DidGetUsage, &done, limited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalUsage(
        base::BindOnce(&DidGetGlobalUsage, &done, usage, unlimited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  void GetHostUsage(const std::string& host, int64_t* usage) {
    bool done = false;
    usage_tracker_.GetHostUsage(host,
                                base::BindOnce(&DidGetUsage, &done, usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> GetHostUsageBreakdown(
      const std::string& host) {
    int64_t usage;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
    bool done = false;

    usage_tracker_.GetHostUsageWithBreakdown(
        host, base::BindOnce(&UsageTrackerTest::DidGetUsageBreakdown, &done,
                             &usage, &usage_breakdown));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(done);
    return std::make_pair(usage, std::move(usage_breakdown));
  }

  void GrantUnlimitedStoragePolicy(const url::Origin& origin) {
    if (!storage_policy_->IsStorageUnlimited(origin.GetURL())) {
      storage_policy_->AddUnlimited(origin.GetURL());
      storage_policy_->NotifyGranted(origin,
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void RevokeUnlimitedStoragePolicy(const url::Origin& origin) {
    if (storage_policy_->IsStorageUnlimited(origin.GetURL())) {
      storage_policy_->RemoveUnlimited(origin.GetURL());
      storage_policy_->NotifyRevoked(origin,
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void SetUsageCacheEnabled(const url::Origin& origin, bool enabled) {
    usage_tracker_.SetUsageCacheEnabled(quota_client_->type(), origin, enabled);
  }

 private:
  std::vector<scoped_refptr<QuotaClient>> GetUsageTrackerList() {
    std::vector<scoped_refptr<QuotaClient>> client_list;
    client_list.push_back(quota_client_);
    return client_list;
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<UsageTrackerTestQuotaClient> quota_client_;
  UsageTracker usage_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTest);
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  int64_t host_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
  const url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  const std::string host(net::GetHostOrSpecFromURL(origin.GetURL()));

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown = GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  int64_t host_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();

  const url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  const std::string host(net::GetHostOrSpecFromURL(origin.GetURL()));

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  UpdateUsageWithoutNotification(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
  host_usage_breakdown = GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(origin);
  UpdateUsageWithoutNotification(origin, 100);
  SetUsageCacheEnabled(origin, false);
  UpdateUsageWithoutNotification(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  EXPECT_EQ(400, host_usage);
  host_usage_breakdown = GetHostUsageBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(400, host_usage);
  host_usage_breakdown = GetHostUsageBreakdown(host);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  SetUsageCacheEnabled(origin, true);
  UpdateUsage(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(500, host_usage);
  host_usage_breakdown = GetHostUsageBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, LimitedGlobalUsageTest) {
  const url::Origin kNormal = url::Origin::Create(GURL("http://normal"));
  const url::Origin kUnlimited = url::Origin::Create(GURL("http://unlimited"));
  const url::Origin kNonCached = url::Origin::Create(GURL("http://non_cached"));
  const url::Origin kNonCachedUnlimited =
      url::Origin::Create(GURL("http://non_cached-unlimited"));

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

}  // namespace storage
