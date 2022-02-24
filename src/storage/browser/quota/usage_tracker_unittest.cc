// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::blink::StorageKey;
using ::blink::mojom::QuotaStatusCode;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

class UsageTrackerTestQuotaClient : public mojom::QuotaClient {
 public:
  UsageTrackerTestQuotaClient() = default;

  UsageTrackerTestQuotaClient(const UsageTrackerTestQuotaClient&) = delete;
  UsageTrackerTestQuotaClient& operator=(const UsageTrackerTestQuotaClient&) =
      delete;

  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, bucket.type);
    int64_t usage = GetUsage(bucket);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), usage));
  }

  void GetStorageKeysForType(StorageType type,
                             GetStorageKeysForTypeCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<StorageKey> storage_keys;
    for (const auto& bucket_usage_pair : bucket_usage_map_)
      storage_keys.emplace(bucket_usage_pair.first.storage_key);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<StorageKey>(storage_keys.begin(),
                                                          storage_keys.end())));
  }

  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, bucket.type);
    bucket_usage_map_.erase(bucket);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), QuotaStatusCode::kOk));
  }

  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override {
    std::move(callback).Run();
  }

  int64_t GetUsage(const BucketLocator& bucket) {
    auto it = bucket_usage_map_.find(bucket);
    if (it == bucket_usage_map_.end())
      return 0;
    return it->second;
  }

  int64_t UpdateUsage(const BucketLocator& bucket, int64_t delta) {
    return bucket_usage_map_[bucket] += delta;
  }

 private:
  std::map<BucketLocator, int64_t> bucket_usage_map_;
};

}  // namespace

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_client_(std::make_unique<UsageTrackerTestQuotaClient>()) {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<QuotaManagerImpl>(
        /*is_incognito=*/false, base_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*quota_change_callback=*/base::DoNothing(), storage_policy_.get(),
        GetQuotaSettingsFunc());
    usage_tracker_ = std::make_unique<UsageTracker>(
        quota_manager_.get(), GetQuotaClientMap(), StorageType::kTemporary,
        storage_policy_.get());
  }

  UsageTrackerTest(const UsageTrackerTest&) = delete;
  UsageTrackerTest& operator=(const UsageTrackerTest&) = delete;

  ~UsageTrackerTest() override = default;

  void UpdateUsage(const BucketInfo& bucket, int64_t delta) {
    quota_client_->UpdateUsage(bucket.ToBucketLocator(), delta);
    usage_tracker_->UpdateBucketUsageCache(QuotaClientType::kFileSystem,
                                           bucket.ToBucketLocator(), delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const BucketInfo& bucket, int64_t delta) {
    quota_client_->UpdateUsage(bucket.ToBucketLocator(), delta);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    base::test::TestFuture<int64_t, int64_t> future;
    usage_tracker_->GetGlobalUsage(future.GetCallback());
    *usage = future.Get<0>();
    *unlimited_usage = future.Get<1>();
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> GetHostUsageWithBreakdown(
      const std::string& host) {
    base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr> future;
    usage_tracker_->GetHostUsageWithBreakdown(host, future.GetCallback());
    return std::make_pair(future.Get<0>(),
                          std::move(std::get<1>(future.Take())));
  }

  void GrantUnlimitedStoragePolicy(const StorageKey& storage_key) {
    if (!storage_policy_->IsStorageUnlimited(storage_key.origin().GetURL())) {
      storage_policy_->AddUnlimited(storage_key.origin().GetURL());
      storage_policy_->NotifyGranted(storage_key.origin(),
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void RevokeUnlimitedStoragePolicy(const StorageKey& storage_key) {
    if (storage_policy_->IsStorageUnlimited(storage_key.origin().GetURL())) {
      storage_policy_->RemoveUnlimited(storage_key.origin().GetURL());
      storage_policy_->NotifyRevoked(storage_key.origin(),
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void SetUsageCacheEnabled(const StorageKey& storage_key, bool enabled) {
    usage_tracker_->SetUsageCacheEnabled(QuotaClientType::kFileSystem,
                                         storage_key, enabled);
  }

  BucketInfo CreateBucket(const StorageKey& storage_key,
                          const std::string& bucket_name) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_->CreateBucketForTesting(storage_key, bucket_name,
                                           StorageType::kTemporary,
                                           future.GetCallback());
    QuotaErrorOr<BucketInfo> bucket_result = future.Take();
    DCHECK(bucket_result.ok());
    return bucket_result.value();
  }

  void OpenDatabase() { quota_manager_->EnsureDatabaseOpened(); }

  void disable_quota_database(bool disable) {
    quota_manager_->database_->SetDisabledForTesting(disable);
  }

  void disable_database_bootstrap(bool disable) {
    quota_manager_->SetBootstrapDisabledForTesting(disable);
  }

 private:
  base::flat_map<mojom::QuotaClient*, QuotaClientType> GetQuotaClientMap() {
    base::flat_map<mojom::QuotaClient*, QuotaClientType> client_map;
    client_map.insert(
        std::make_pair(quota_client_.get(), QuotaClientType::kFileSystem));
    return client_map;
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  std::unique_ptr<UsageTrackerTestQuotaClient> quota_client_;

  scoped_refptr<QuotaManagerImpl> quota_manager_;
  std::unique_ptr<UsageTracker> usage_tracker_;
  base::ScopedTempDir base_;
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const std::string& host = storage_key.origin().host();

  BucketInfo bucket = CreateBucket(storage_key, kDefaultBucketName);

  UpdateUsage(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const std::string& host = storage_key.origin().host();

  BucketInfo bucket = CreateBucket(storage_key, kDefaultBucketName);

  UpdateUsage(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  UpdateUsageWithoutNotification(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  UpdateUsageWithoutNotification(bucket, 100);
  SetUsageCacheEnabled(storage_key, false);
  UpdateUsageWithoutNotification(bucket, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  SetUsageCacheEnabled(storage_key, true);
  UpdateUsage(bucket, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(500, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, GlobalUsageUnlimitedUncached) {
  const StorageKey kNormal =
      StorageKey::CreateFromStringForTesting("http://normal");
  const StorageKey kUnlimited =
      StorageKey::CreateFromStringForTesting("http://unlimited");
  const StorageKey kNonCached =
      StorageKey::CreateFromStringForTesting("http://non_cached");
  const StorageKey kNonCachedUnlimited =
      StorageKey::CreateFromStringForTesting("http://non_cached-unlimited");

  BucketInfo bucket_normal = CreateBucket(kNormal, kDefaultBucketName);
  BucketInfo bucket_unlimited = CreateBucket(kUnlimited, kDefaultBucketName);
  BucketInfo bucket_noncached = CreateBucket(kNonCached, kDefaultBucketName);
  BucketInfo bucket_noncached_unlimited =
      CreateBucket(kNonCachedUnlimited, kDefaultBucketName);

  GrantUnlimitedStoragePolicy(kUnlimited);
  GrantUnlimitedStoragePolicy(kNonCachedUnlimited);

  SetUsageCacheEnabled(kNonCached, false);
  SetUsageCacheEnabled(kNonCachedUnlimited, false);

  UpdateUsageWithoutNotification(bucket_normal, 1);
  UpdateUsageWithoutNotification(bucket_unlimited, 2);
  UpdateUsageWithoutNotification(bucket_noncached, 4);
  UpdateUsageWithoutNotification(bucket_noncached_unlimited, 8);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 4 + 8, total_usage);
  EXPECT_EQ(2 + 8, unlimited_usage);

  UpdateUsageWithoutNotification(bucket_noncached, 16 - 4);
  UpdateUsageWithoutNotification(bucket_noncached_unlimited, 32 - 8);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 16 + 32, total_usage);
  EXPECT_EQ(2 + 32, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostCachedInit) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  BucketInfo bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketInfo bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the storage_keys before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostCachedUpdate) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  BucketInfo bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketInfo bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the storage keys after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  UpdateUsage(bucket1, 100);
  UpdateUsage(bucket2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostUncachedInit) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  BucketInfo bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketInfo bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  SetUsageCacheEnabled(kStorageKey1, false);
  SetUsageCacheEnabled(kStorageKey2, false);

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the storage keys before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostUncachedUpdate) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  BucketInfo bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketInfo bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the storage keys after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  SetUsageCacheEnabled(kStorageKey1, false);
  SetUsageCacheEnabled(kStorageKey2, false);

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, QuotaDatabaseDisabled) {
  disable_database_bootstrap(true);
  OpenDatabase();

  disable_quota_database(true);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(total_usage, -1);
  EXPECT_EQ(unlimited_usage, -1);

  const StorageKey kStorageKey =
      StorageKey::CreateFromStringForTesting("http://example.com");
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(kStorageKey.origin().host());
  EXPECT_EQ(host_usage_breakdown.first, -1);
  EXPECT_EQ(host_usage_breakdown.second, blink::mojom::UsageBreakdown::New());
}

}  // namespace storage
