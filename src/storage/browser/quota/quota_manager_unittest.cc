// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/test/mock_quota_client.h"
#include "storage/browser/test/mock_quota_database.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

using ::blink::StorageKey;
using ::blink::mojom::QuotaStatusCode;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

// For shorter names.
const StorageType kTemp = StorageType::kTemporary;
const StorageType kPerm = StorageType::kPersistent;
const StorageType kSync = StorageType::kSyncable;

// Values in bytes.
const int64_t kAvailableSpaceForApp = 13377331U;
const int64_t kMustRemainAvailableForSystem = kAvailableSpaceForApp / 2;
const int64_t kDefaultPoolSize = 1000;
const int64_t kDefaultPerHostQuota = 200;
const int64_t kGigabytes = QuotaManagerImpl::kGBytes;

struct UsageAndQuotaResult {
  QuotaStatusCode status;
  int64_t usage;
  int64_t quota;
};

struct GlobalUsageResult {
  int64_t usage;
  int64_t unlimited_usage;
};

struct StorageCapacityResult {
  int64_t total_space;
  int64_t available_space;
};

struct ClientBucketData {
  const char* origin;
  std::string name;
  StorageType type;
  int64_t usage;
};

// Returns a deterministic value for the amount of available disk space.
int64_t GetAvailableDiskSpaceForTest() {
  return kAvailableSpaceForApp + kMustRemainAvailableForSystem;
}

std::tuple<int64_t, int64_t> GetVolumeInfoForTests(
    const base::FilePath& unused) {
  int64_t available = static_cast<uint64_t>(GetAvailableDiskSpaceForTest());
  int64_t total = available * 2;
  return std::make_tuple(total, available);
}

StorageKey ToStorageKey(const std::string& url) {
  return StorageKey::CreateFromStringForTesting(url);
}

MATCHER_P3(MatchesBucketTableEntry, storage_key, type, use_count, "") {
  return testing::ExplainMatchResult(storage_key, arg.storage_key,
                                     result_listener) &&
         testing::ExplainMatchResult(type, arg.type, result_listener) &&
         testing::ExplainMatchResult(use_count, arg.use_count, result_listener);
}

}  // namespace

class QuotaManagerImplTest : public testing::Test {
 protected:
  using QuotaTableEntry = QuotaManagerImpl::QuotaTableEntry;
  using QuotaTableEntries = QuotaManagerImpl::QuotaTableEntries;
  using BucketTableEntries = QuotaManagerImpl::BucketTableEntries;

 public:
  QuotaManagerImplTest() : mock_time_counter_(0) {}

  QuotaManagerImplTest(const QuotaManagerImplTest&) = delete;
  QuotaManagerImplTest& operator=(const QuotaManagerImplTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    mock_special_storage_policy_ =
        base::MakeRefCounted<MockSpecialStoragePolicy>();
    ResetQuotaManagerImpl(false /* is_incognito */);
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    quota_manager_impl_ = nullptr;
    task_environment_.RunUntilIdle();
  }

 protected:
  void ResetQuotaManagerImpl(bool is_incognito) {
    quota_manager_impl_ = base::MakeRefCounted<QuotaManagerImpl>(
        is_incognito, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*quota_change_callback=*/base::DoNothing(),
        mock_special_storage_policy_.get(), GetQuotaSettingsFunc());
    SetQuotaSettings(kDefaultPoolSize, kDefaultPerHostQuota,
                     is_incognito ? INT64_C(0) : kMustRemainAvailableForSystem);

    // Don't (automatically) start the eviction for testing.
    quota_manager_impl_->eviction_disabled_ = true;
    // Don't query the hard disk for remaining capacity.
    quota_manager_impl_->get_volume_info_fn_ = &GetVolumeInfoForTests;
    additional_callback_count_ = 0;
  }

  MockQuotaClient* CreateAndRegisterClient(
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType> storage_types,
      base::span<const UnmigratedStorageKeyData> unmigrated_data =
          base::span<const UnmigratedStorageKeyData>()) {
    auto mock_quota_client = std::make_unique<storage::MockQuotaClient>(
        quota_manager_impl_->proxy(), client_type, unmigrated_data);
    MockQuotaClient* mock_quota_client_ptr = mock_quota_client.get();

    mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
    mojo::MakeSelfOwnedReceiver(std::move(mock_quota_client),
                                quota_client.InitWithNewPipeAndPassReceiver());
    quota_manager_impl_->proxy()->RegisterClient(std::move(quota_client),
                                                 client_type, storage_types);
    return mock_quota_client_ptr;
  }

  // Creates buckets in QuotaDatabase if they don't exist yet, and sets usage
  // to the `client`.
  void RegisterClientBucketData(MockQuotaClient* client,
                                base::span<const ClientBucketData> mock_data) {
    std::map<BucketLocator, int64_t> buckets_data;
    for (const ClientBucketData& data : mock_data) {
      base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
      quota_manager_impl_->GetOrCreateBucketDeprecated(
          ToStorageKey(data.origin), data.name, data.type,
          future.GetCallback());
      auto bucket = future.Take();
      EXPECT_TRUE(bucket.ok());
      buckets_data.insert(std::pair<BucketLocator, int64_t>(
          bucket->ToBucketLocator(), data.usage));
    }
    client->AddBucketsData(buckets_data);
  }

  void OpenDatabase() { quota_manager_impl_->EnsureDatabaseOpened(); }

  QuotaErrorOr<BucketInfo> GetOrCreateBucket(const StorageKey& storage_key,
                                             const std::string& bucket_name) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->GetOrCreateBucket(storage_key, bucket_name,
                                           future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<BucketInfo> CreateBucketForTesting(
      const StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->CreateBucketForTesting(
        storage_key, bucket_name, storage_type, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<BucketInfo> GetBucket(const StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->GetBucket(storage_key, bucket_name, storage_type,
                                   future.GetCallback());
    return future.Take();
  }

  std::set<StorageKey> GetStorageKeysForType(
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<std::set<StorageKey>> future;
    quota_manager_impl_->GetStorageKeysForType(
        storage_type, future.GetCallback<const std::set<StorageKey>&>());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForType(
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketLocator>>> future;
    quota_manager_impl_->GetBucketsForType(storage_type, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForHost(
      const std::string& host,
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketLocator>>> future;
    quota_manager_impl_->GetBucketsForHost(host, storage_type,
                                           future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForStorageKey(
      const StorageKey& storage_key,
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketLocator>>> future;
    quota_manager_impl_->GetBucketsForStorageKey(storage_key, storage_type,
                                                 future.GetCallback());
    return future.Take();
  }

  UsageAndQuotaResult GetUsageAndQuotaForWebApps(const StorageKey& storage_key,
                                                 StorageType type) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t> future;
    quota_manager_impl_->GetUsageAndQuotaForWebApps(storage_key, type,
                                                    future.GetCallback());
    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  void GetUsageAndQuotaWithBreakdown(const StorageKey& storage_key,
                                     StorageType type) {
    base::RunLoop run_loop;
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    usage_breakdown_ = nullptr;
    quota_manager_impl_->GetUsageAndQuotaWithBreakdown(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuotaWithBreakdown,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  UsageAndQuotaResult GetUsageAndQuotaForStorageClient(
      const StorageKey& storage_key,
      StorageType type) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t> future;
    quota_manager_impl_->GetUsageAndQuota(storage_key, type,
                                          future.GetCallback());
    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  void SetQuotaSettings(int64_t pool_size,
                        int64_t per_host_quota,
                        int64_t must_remain_available) {
    QuotaSettings settings;
    settings.pool_size = pool_size;
    settings.per_host_quota = per_host_quota;
    settings.session_only_per_host_quota =
        (per_host_quota > 0) ? (per_host_quota - 1) : 0;
    settings.must_remain_available = must_remain_available;
    settings.refresh_interval = base::TimeDelta::Max();
    quota_manager_impl_->SetQuotaSettings(settings);
  }

  using GetVolumeInfoFn =
      std::tuple<int64_t, int64_t> (*)(const base::FilePath&);

  void SetGetVolumeInfoFn(GetVolumeInfoFn fn) {
    quota_manager_impl_->SetGetVolumeInfoFnForTesting(fn);
  }

  int64_t GetPersistentHostQuota(const std::string& host) {
    base::test::TestFuture<QuotaStatusCode, int64_t> future;
    quota_manager_impl_->GetPersistentHostQuota(host, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), QuotaStatusCode::kOk);
    return future.Get<1>();
  }

  int64_t SetPersistentHostQuota(const std::string& host, int64_t new_quota) {
    base::test::TestFuture<QuotaStatusCode, int64_t> future;
    quota_manager_impl_->SetPersistentHostQuota(host, new_quota,
                                                future.GetCallback());
    EXPECT_EQ(future.Get<0>(), QuotaStatusCode::kOk);
    return future.Get<1>();
  }

  GlobalUsageResult GetGlobalUsage(StorageType type) {
    base::test::TestFuture<int64_t, int64_t> future;
    quota_manager_impl_->GetGlobalUsage(type, future.GetCallback());
    return {future.Get<0>(), future.Get<1>()};
  }

  void GetHostUsageWithBreakdown(const std::string& host, StorageType type) {
    base::RunLoop run_loop;
    usage_ = -1;
    quota_manager_impl_->GetHostUsageWithBreakdown(
        host, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetHostUsageBreakdown,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void RunAdditionalUsageAndQuotaTask(const StorageKey& storage_key,
                                      StorageType type) {
    quota_manager_impl_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuotaAdditional,
                       weak_factory_.GetWeakPtr()));
  }

  QuotaStatusCode EvictBucketData(const BucketLocator& bucket) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->EvictBucketData(bucket, future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode DeleteBucketData(const BucketLocator& bucket,
                                   QuotaClientTypes quota_client_types) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->DeleteBucketData(bucket, std::move(quota_client_types),
                                          future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode DeleteHostData(const std::string& host, StorageType type) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->DeleteHostData(host, type, future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode FindAndDeleteBucketData(const StorageKey& storage_key,
                                          const std::string& bucket_name) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->FindAndDeleteBucketData(storage_key, bucket_name,
                                                 future.GetCallback());
    return future.Get();
  }

  StorageCapacityResult GetStorageCapacity() {
    base::test::TestFuture<int64_t, int64_t> future;
    quota_manager_impl_->GetStorageCapacity(future.GetCallback());
    return {future.Get<0>(), future.Get<1>()};
  }

  void GetEvictionRoundInfo() {
    quota_status_ = QuotaStatusCode::kUnknown;
    settings_ = QuotaSettings();
    available_space_ = -1;
    total_space_ = -1;
    usage_ = -1;
    quota_manager_impl_->GetEvictionRoundInfo(
        base::BindOnce(&QuotaManagerImplTest::DidGetEvictionRoundInfo,
                       weak_factory_.GetWeakPtr()));
  }

  void NotifyStorageAccessed(const StorageKey& storage_key, StorageType type) {
    quota_manager_impl_->NotifyStorageAccessed(storage_key, type,
                                               IncrementMockTime());
  }

  void NotifyBucketAccessed(BucketId bucket_id) {
    quota_manager_impl_->NotifyBucketAccessed(bucket_id, IncrementMockTime());
  }

  void GetEvictionBucket(StorageType type) {
    eviction_bucket_.reset();
    // The quota manager's default eviction policy is to use an LRU eviction
    // policy.
    quota_manager_impl_->GetEvictionBucket(
        type, base::BindOnce(&QuotaManagerImplTest::DidGetEvictionBucket,
                             weak_factory_.GetWeakPtr()));
  }

  std::set<BucketLocator> GetBucketsModifiedBetween(StorageType type,
                                                    base::Time begin,
                                                    base::Time end) {
    base::test::TestFuture<std::set<BucketLocator>, StorageType> future;
    quota_manager_impl_->GetBucketsModifiedBetween(
        type, begin, end,
        future.GetCallback<const std::set<BucketLocator>&, StorageType>());
    EXPECT_EQ(future.Get<1>(), type);
    return future.Get<0>();
  }

  QuotaTableEntries DumpQuotaTable() {
    base::test::TestFuture<QuotaTableEntries> future;
    quota_manager_impl_->DumpQuotaTable(
        future.GetCallback<const QuotaTableEntries&>());
    return future.Get();
  }

  BucketTableEntries DumpBucketTable() {
    base::test::TestFuture<BucketTableEntries> future;
    quota_manager_impl_->DumpBucketTable(
        future.GetCallback<const BucketTableEntries&>());
    return future.Get();
  }

  void DidGetUsageAndQuotaWithBreakdown(
      base::OnceClosure quit_closure,
      QuotaStatusCode status,
      int64_t usage,
      int64_t quota,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
    usage_breakdown_ = std::move(usage_breakdown);
    std::move(quit_closure).Run();
  }

  void DidGetHostUsageBreakdown(
      base::OnceClosure quit_closure,
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    usage_ = usage;
    usage_breakdown_ = std::move(usage_breakdown);
    std::move(quit_closure).Run();
  }

  void DidGetEvictionRoundInfo(QuotaStatusCode status,
                               const QuotaSettings& settings,
                               int64_t available_space,
                               int64_t total_space,
                               int64_t global_usage,
                               bool global_usage_is_complete) {
    quota_status_ = status;
    settings_ = settings;
    available_space_ = available_space;
    total_space_ = total_space;
    usage_ = global_usage;
  }

  void DidGetEvictionBucket(const absl::optional<BucketLocator>& bucket) {
    eviction_bucket_ = bucket;
    DCHECK(!bucket.has_value() ||
           !bucket->storage_key.origin().GetURL().is_empty());
  }

  void GetUsage_WithModifyTestBody(const StorageType type);

  void SetStoragePressureCallback(
      base::RepeatingCallback<void(StorageKey)> callback) {
    quota_manager_impl_->SetStoragePressureCallback(std::move(callback));
  }

  void MaybeRunStoragePressureCallback(const StorageKey& storage_key,
                                       int64_t total,
                                       int64_t available) {
    quota_manager_impl_->MaybeRunStoragePressureCallback(storage_key, total,
                                                         available);
  }

  void set_additional_callback_count(int c) { additional_callback_count_ = c; }
  int additional_callback_count() const { return additional_callback_count_; }
  void DidGetUsageAndQuotaAdditional(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    ++additional_callback_count_;
  }

  QuotaManagerImpl* quota_manager_impl() const {
    return quota_manager_impl_.get();
  }

  void set_quota_manager_impl(QuotaManagerImpl* quota_manager_impl) {
    quota_manager_impl_ = quota_manager_impl;
  }

  MockSpecialStoragePolicy* mock_special_storage_policy() const {
    return mock_special_storage_policy_.get();
  }

  std::unique_ptr<QuotaOverrideHandle> GetQuotaOverrideHandle() {
    return quota_manager_impl_->proxy()->GetQuotaOverrideHandle();
  }

  void SetQuotaChangeCallback(base::RepeatingClosure cb) {
    quota_manager_impl_->SetQuotaChangeCallbackForTesting(std::move(cb));
  }

  void SetQuotaDatabase(std::unique_ptr<QuotaDatabase> database) {
    quota_manager_impl_->SetQuotaDatabaseForTesting(std::move(database));
  }

  bool is_db_bootstrapping() {
    return quota_manager_impl_->is_bootstrapping_database_for_testing();
  }

  bool is_db_disabled() {
    return quota_manager_impl_->is_db_disabled_for_testing();
  }

  void disable_quota_database(bool disable) {
    quota_manager_impl_->database_->SetDisabledForTesting(disable);
  }

  void disable_database_bootstrap(bool disable) {
    quota_manager_impl_->SetBootstrapDisabledForTesting(disable);
  }

  QuotaStatusCode status() const { return quota_status_; }
  int64_t usage() const { return usage_; }
  const blink::mojom::UsageBreakdown& usage_breakdown() const {
    return *usage_breakdown_;
  }
  int64_t quota() const { return quota_; }
  int64_t total_space() const { return total_space_; }
  int64_t available_space() const { return available_space_; }
  const absl::optional<BucketLocator>& eviction_bucket() const {
    return eviction_bucket_;
  }
  const QuotaSettings& settings() const { return settings_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<QuotaManagerImpl> quota_manager_impl_;

 private:
  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
  }

  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  int64_t usage_;
  blink::mojom::UsageBreakdownPtr usage_breakdown_;
  int64_t quota_;
  int64_t total_space_;
  int64_t available_space_;
  absl::optional<BucketLocator> eviction_bucket_;
  QuotaSettings settings_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerImplTest> weak_factory_{this};
};

TEST_F(QuotaManagerImplTest, QuotaDatabaseBootstrap) {
  static const UnmigratedStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 15},
      {"http://bar.com/", kPerm, 50},
  };
  static const UnmigratedStorageKeyData kData2[] = {
      {"https://foo.com/", kTemp, 30},
      {"https://foo.com:8081/", kTemp, 35},
      {"http://example.com/", kPerm, 40},
  };
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm}, kData1);
  CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm}, kData2);

  // OpenDatabase should trigger database bootstrapping.
  OpenDatabase();
  EXPECT_TRUE(is_db_bootstrapping());

  // When bootstrapping is complete, queued calls to the QuotaDatabase
  // should return successfully and buckets for registered storage keys should
  // already exist.
  auto bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  EXPECT_FALSE(is_db_bootstrapping());
  ASSERT_TRUE(bucket.ok());

  bucket = GetBucket(ToStorageKey("http://foo.com:8080/"), kDefaultBucketName,
                     kTemp);
  ASSERT_TRUE(bucket.ok());

  bucket = GetBucket(ToStorageKey("https://foo.com:8081/"), kDefaultBucketName,
                     kTemp);
  ASSERT_TRUE(bucket.ok());

  bucket =
      GetBucket(ToStorageKey("http://bar.com/"), kDefaultBucketName, kPerm);
  ASSERT_TRUE(bucket.ok());

  bucket =
      GetBucket(ToStorageKey("http://example.com/"), kDefaultBucketName, kPerm);
  ASSERT_TRUE(bucket.ok());
}

TEST_F(QuotaManagerImplTest, GetUsageInfo) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 15},
      {"http://bar.com/", "logs", kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kPerm, 50},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 30},
      {"https://foo.com:8081/", kDefaultBucketName, kTemp, 35},
      {"http://bar.com/", kDefaultBucketName, kPerm, 40},
      {"http://example.com/", kDefaultBucketName, kPerm, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* database_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(database_client, kData2);

  base::test::TestFuture<UsageInfoEntries> future;
  quota_manager_impl()->GetUsageInfo(future.GetCallback());
  auto entries = future.Get();

  EXPECT_THAT(entries, testing::UnorderedElementsAre(
                           UsageInfo("foo.com", kTemp, 10 + 15 + 30 + 35),
                           UsageInfo("bar.com", kTemp, 20),
                           UsageInfo("bar.com", kPerm, 40 + 50),
                           UsageInfo("example.com", kPerm, 40)));
}

TEST_F(QuotaManagerImplTest, DatabaseDisabledAfterThreshold) {
  disable_database_bootstrap(true);
  OpenDatabase();

  // Disable quota database for database error behavior.
  disable_quota_database(true);

  ASSERT_FALSE(is_db_disabled());

  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  auto bucket = GetOrCreateBucket(storage_key, bucket_name);
  ASSERT_FALSE(bucket.ok());
  ASSERT_FALSE(is_db_disabled());

  bucket = GetOrCreateBucket(storage_key, bucket_name);
  ASSERT_FALSE(bucket.ok());
  ASSERT_FALSE(is_db_disabled());

  // Disables access to QuotaDatabase after error counts passes threshold.
  bucket = GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_FALSE(bucket.ok());
  ASSERT_TRUE(is_db_disabled());
}

TEST_F(QuotaManagerImplTest, GetOrCreateBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  auto bucket = GetOrCreateBucket(storage_key, bucket_name);
  ASSERT_TRUE(bucket.ok());

  BucketId created_bucket_id = bucket.value().id;

  bucket = GetOrCreateBucket(storage_key, bucket_name);
  EXPECT_TRUE(bucket.ok());
  EXPECT_EQ(bucket.value().id, created_bucket_id);
}

TEST_F(QuotaManagerImplTest, GetBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  auto bucket = CreateBucketForTesting(storage_key, bucket_name, kTemp);
  ASSERT_TRUE(bucket.ok());
  BucketInfo created_bucket = bucket.value();

  bucket = GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_TRUE(bucket.ok());
  BucketInfo retrieved_bucket = bucket.value();
  EXPECT_EQ(created_bucket.id, retrieved_bucket.id);

  bucket = GetBucket(storage_key, "bucket_b", kTemp);
  ASSERT_FALSE(bucket.ok());
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);
  ASSERT_FALSE(is_db_disabled());
}

TEST_F(QuotaManagerImplTest, GetStorageKeysForType) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  auto bucket = CreateBucketForTesting(storage_key_a, "bucket_a", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_a = bucket.value();

  bucket = CreateBucketForTesting(storage_key_b, "bucket_b", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_b = bucket.value();

  bucket = CreateBucketForTesting(storage_key_c, "bucket_c", kPerm);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_c = bucket.value();

  std::set<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_THAT(storage_keys,
              testing::UnorderedElementsAre(storage_key_a, storage_key_b));

  storage_keys = GetStorageKeysForType(kPerm);
  EXPECT_THAT(storage_keys, testing::UnorderedElementsAre(storage_key_c));
}

TEST_F(QuotaManagerImplTest, GetStorageKeysForTypeWithDatabaseError) {
  disable_database_bootstrap(true);
  OpenDatabase();

  // Disable quota database for database error behavior.
  disable_quota_database(true);

  // Return empty set when error is encountered.
  std::set<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_TRUE(storage_keys.empty());
}

TEST_F(QuotaManagerImplTest, GetBucketsForType) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  auto bucket = CreateBucketForTesting(storage_key_a, "bucket_a", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_a = bucket.value();

  bucket = CreateBucketForTesting(storage_key_b, "bucket_b", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_b = bucket.value();

  bucket = CreateBucketForTesting(storage_key_c, kDefaultBucketName, kPerm);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_c = bucket.value();

  QuotaErrorOr<std::set<BucketLocator>> result = GetBucketsForType(kTemp);
  EXPECT_TRUE(result.ok());

  std::set<BucketLocator> buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_a.ToBucketLocator()));
  EXPECT_THAT(buckets, testing::Contains(bucket_b.ToBucketLocator()));

  result = GetBucketsForType(kPerm);
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_c.ToBucketLocator()));
}

TEST_F(QuotaManagerImplTest, GetBucketsForHost) {
  StorageKey host_a_storage_key_1 = ToStorageKey("http://a.com/");
  StorageKey host_a_storage_key_2 = ToStorageKey("https://a.com:123/");
  StorageKey host_b_storage_key = ToStorageKey("http://b.com/");

  auto bucket =
      CreateBucketForTesting(host_a_storage_key_1, kDefaultBucketName, kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo host_a_bucket_1 = bucket.value();

  bucket = CreateBucketForTesting(host_a_storage_key_2, "test", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo host_a_bucket_2 = bucket.value();

  bucket =
      CreateBucketForTesting(host_b_storage_key, kDefaultBucketName, kPerm);
  EXPECT_TRUE(bucket.ok());
  BucketInfo host_b_bucket = bucket.value();

  QuotaErrorOr<std::set<BucketLocator>> result =
      GetBucketsForHost("a.com", kTemp);
  EXPECT_TRUE(result.ok());

  std::set<BucketLocator> buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(host_a_bucket_1.ToBucketLocator()));
  EXPECT_THAT(buckets, testing::Contains(host_a_bucket_2.ToBucketLocator()));

  result = GetBucketsForHost("b.com", kPerm);
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(host_b_bucket.ToBucketLocator()));
}

TEST_F(QuotaManagerImplTest, GetBucketsForStorageKey) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  auto bucket = CreateBucketForTesting(storage_key_a, "bucket_a1", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_a1 = bucket.value();

  bucket = CreateBucketForTesting(storage_key_a, "bucket_a2", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_a2 = bucket.value();

  bucket = CreateBucketForTesting(storage_key_b, "bucket_b", kTemp);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_b = bucket.value();

  bucket = CreateBucketForTesting(storage_key_c, kDefaultBucketName, kPerm);
  EXPECT_TRUE(bucket.ok());
  BucketInfo bucket_c = bucket.value();

  QuotaErrorOr<std::set<BucketLocator>> result =
      GetBucketsForStorageKey(storage_key_a, kTemp);
  EXPECT_TRUE(result.ok());

  std::set<BucketLocator> buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_a1.ToBucketLocator()));
  EXPECT_THAT(buckets, testing::Contains(bucket_a2.ToBucketLocator()));

  result = GetBucketsForStorageKey(storage_key_a, kPerm);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());

  result = GetBucketsForStorageKey(storage_key_c, kPerm);
  EXPECT_TRUE(result.ok());

  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_c.ToBucketLocator()));
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Simple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", "logs", kTemp, 10},
      {"http://foo.com/", kDefaultBucketName, kPerm, 80},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 80);
  EXPECT_EQ(result.quota, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_GT(result.quota, 0);
  int64_t quota_returned_for_foo = result.quota;

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, quota_returned_for_foo);
}

TEST_F(QuotaManagerImplTest, GetUsage_NoClient) {
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(0, usage());

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetUsage_EmptyClient) {
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(0, usage());

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_MultiStorageKeys) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", "logs", kTemp, 5},
      {"https://bar.com/", "notes", kTemp, 7},
      {"http://baz.com/", "songs", kTemp, 30},
      {"http://foo.com/", kDefaultBucketName, kPerm, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  // This time explicitly sets a temporary global quota.
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);

  // The host's quota should be its full portion of the global quota
  // since there's plenty of diskspace.
  EXPECT_EQ(result.quota, kPerHostQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 5 + 7);
  EXPECT_EQ(result.quota, kPerHostQuota);
}

TEST_F(QuotaManagerImplTest, GetUsage_MultipleClients) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 2},
      {"http://bar.com/", kDefaultBucketName, kPerm, 4},
      {"http://unlimited/", kDefaultBucketName, kPerm, 8},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 128},
      {"http://example.com/", kDefaultBucketName, kPerm, 256},
      {"http://unlimited/", "logs", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto storage_capacity = GetStorageCapacity();

  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* database_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(database_client, kData2);

  const int64_t kPoolSize = GetAvailableDiskSpaceForTest();
  const int64_t kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 1 + 128);
  EXPECT_EQ(result.quota, kPerHostQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4);
  EXPECT_EQ(result.quota, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 512);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 8);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 1 + 2 + 128 + 512);
  EXPECT_EQ(global_usage_result.unlimited_usage, 512);

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 4 + 8 + 256);
  EXPECT_EQ(global_usage_result.unlimited_usage, 8);
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_Simple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com/", kDefaultBucketName, kPerm, 80},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);
  RegisterClientBucketData(sw_client, kData3);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  usage_breakdown_expected.fileSystem = 80;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.serviceWorkerCache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 4 + 8, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 4;
  usage_breakdown_expected.serviceWorkerCache = 8;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.serviceWorkerCache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_NoClient) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultiStorageKeys) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", "logs", kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 5},
      {"https://bar.com/", kDefaultBucketName, kTemp, 7},
      {"http://baz.com/", "logs", kTemp, 30},
      {"http://foo.com/", kDefaultBucketName, kPerm, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  usage_breakdown_expected.fileSystem = 10 + 20;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  usage_breakdown_expected.fileSystem = 5 + 7;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultipleClients) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 2},
      {"http://bar.com/", kDefaultBucketName, kPerm, 4},
      {"http://unlimited/", kDefaultBucketName, kPerm, 8},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 128},
      {"http://example.com/", kDefaultBucketName, kPerm, 256},
      {"http://unlimited/", "logs", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 128;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kPerm);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  usage_breakdown_expected.fileSystem = 4;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 512;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kPerm);
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  usage_breakdown_expected.fileSystem = 8;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

void QuotaManagerImplTest::GetUsage_WithModifyTestBody(const StorageType type) {
  const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, type, 10},
      {"http://bar.com/", kDefaultBucketName, type, 0},
      {"http://foo.com:1/", kDefaultBucketName, type, 20},
      {"https://foo.com/", kDefaultBucketName, type, 0},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {type});
  RegisterClientBucketData(client, kData);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), type);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), type, 30);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com:1/"), type,
                                    -5);
  client->ModifyStorageKeyAndNotify(ToStorageKey("https://foo.com/"), type, 1);

  // Database call to ensure modification calls have completed.
  GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), type);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20 + 30 - 5 + 1);
  int foo_usage = result.usage;

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://bar.com/"), type, 40);

  // Database call to ensure modification calls have completed.
  GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), type);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 40);

  auto global_usage_result = GetGlobalUsage(type);
  EXPECT_EQ(global_usage_result.usage, foo_usage + 40);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsage_WithModify) {
  GetUsage_WithModifyTestBody(kTemp);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", "logs", kTemp, 13},
      {"http://foo.com/", "inbox", kPerm, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);
  EXPECT_EQ(result.quota, kPerHostQuota);

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);
  EXPECT_EQ(result.quota, kPerHostQuota);
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 13},
      {"http://foo.com/", kDefaultBucketName, kPerm, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  set_additional_callback_count(0);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);

  base::test::TestFuture<QuotaStatusCode> future_foo;
  base::test::TestFuture<QuotaStatusCode> future_bar;
  quota_manager_impl()->DeleteHostData("foo.com", kTemp,
                                       future_foo.GetCallback());
  quota_manager_impl()->DeleteHostData("bar.com", kTemp,
                                       future_bar.GetCallback());

  // Nuke before waiting for callbacks.
  set_quota_manager_impl(nullptr);

  EXPECT_EQ(QuotaStatusCode::kErrorAbort, future_foo.Get());
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, future_bar.Get());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const ClientBucketData kData[] = {
      {"http://usage1/", kDefaultBucketName, kTemp, 1},
      {"http://usage10/", kDefaultBucketName, kTemp, 10},
      {"http://usage200/", kDefaultBucketName, kTemp, 200},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  // Provided diskspace is not tight, global usage does not affect the
  // quota calculations for an individual storage key, so despite global usage
  // in excess of our poolsize, we still get the nominal quota value.
  auto storage_capacity = GetStorageCapacity();
  EXPECT_LE(kMustRemainAvailableForSystem, storage_capacity.available_space);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://usage1/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 1);
  EXPECT_EQ(result.quota, kPerHostQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerHostQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage200/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 200);
  // Should be clamped to the nominal quota.
  EXPECT_EQ(result.quota, kPerHostQuota);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const ClientBucketData kData[] = {
      {"http://usage10/", kDefaultBucketName, kTemp, 10},
      {"http://usage50/", kDefaultBucketName, kTemp, 50},
      {"http://unlimited/", "inbox", kTemp, 4000},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto storage_capacity = GetStorageCapacity();
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  // Test when not overbugdet.
  const int kPerHostQuotaFor1000 = 200;
  SetQuotaSettings(1000, kPerHostQuotaFor1000, kMustRemainAvailableForSystem);
  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 10 + 50 + 4000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 4000);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerHostQuotaFor1000);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerHostQuotaFor1000);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"),
                                            kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, QuotaManagerImpl::kNoLimit);

  // Test when overbudgeted.
  const int kPerHostQuotaFor100 = 20;
  SetQuotaSettings(100, kPerHostQuotaFor100, kMustRemainAvailableForSystem);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"),
                                            kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, QuotaManagerImpl::kNoLimit);

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 10 + 50 + 4000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);

  result = GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"),
                                            kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, kPerHostQuotaFor100);
}

TEST_F(QuotaManagerImplTest, GetAndSetPerststentHostQuota) {
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});

  EXPECT_EQ(GetPersistentHostQuota("foo.com"), 0);
  EXPECT_EQ(SetPersistentHostQuota("foo.com", 100), 100);

  // Should still succeed after multiple calls at once.
  base::test::TestFuture<QuotaStatusCode, int64_t> future1;
  base::test::TestFuture<QuotaStatusCode, int64_t> future2;
  base::test::TestFuture<QuotaStatusCode, int64_t> future3;
  quota_manager_impl()->SetPersistentHostQuota("foo.com", 200,
                                               future1.GetCallback());
  quota_manager_impl()->SetPersistentHostQuota("foo.com", 300,
                                               future2.GetCallback());
  quota_manager_impl()->SetPersistentHostQuota(
      "foo.com", QuotaManagerImpl::kPerHostPersistentQuotaLimit,
      future3.GetCallback());
  EXPECT_EQ(GetPersistentHostQuota("foo.com"),
            QuotaManagerImpl::kPerHostPersistentQuotaLimit);

  // Persistent quota should be capped at the per-host quota limit.
  SetPersistentHostQuota("foo.com",
                         QuotaManagerImpl::kPerHostPersistentQuotaLimit + 100);
  EXPECT_EQ(GetPersistentHostQuota("foo.com"),
            QuotaManagerImpl::kPerHostPersistentQuotaLimit);
}

TEST_F(QuotaManagerImplTest, GetAndSetPersistentUsageAndQuota) {
  auto storage_capacity = GetStorageCapacity();
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, 0);

  SetPersistentHostQuota("foo.com", 100);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, 100);

  // The actual space available is given to 'unlimited' storage keys as their
  // quota.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kPerm);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"),
                                            kPerm);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, QuotaManagerImpl::kNoLimit);
}

TEST_F(QuotaManagerImplTest, GetQuotaLowAvailableDiskSpace) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 100000},
      {"http://unlimited/", kDefaultBucketName, kTemp, 4000000},
  };

  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;

  // In here, we expect the low available space logic branch
  // to be ignored. Doing so should have QuotaManagerImpl return the same
  // per-host quota as what is set in QuotaSettings, despite being in a state of
  // low available space.
  const int kMustRemainAvailable =
      static_cast<int>(GetAvailableDiskSpaceForTest() - 65536);
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailable);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 100000);
  EXPECT_EQ(result.quota, kPerHostQuota);
}

TEST_F(QuotaManagerImplTest, GetSyncableQuota) {
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});

  // Pre-condition check: available disk space (for testing) is less than
  // the default quota for syncable storage.
  EXPECT_LE(kAvailableSpaceForApp,
            QuotaManagerImpl::kSyncableStorageDefaultHostQuota);

  // The quota manager should return
  // QuotaManagerImpl::kSyncableStorageDefaultHostQuota as syncable quota,
  // despite available space being less than the desired quota. Only
  // storage keys with unlimited storage, which is never the case for syncable
  // storage, shall have their quota calculation take into account the amount of
  // available disk space.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, QuotaManagerImpl::kSyncableStorageDefaultHostQuota);
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_MultiStorageKeys) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kPerm, 20},
      {"https://foo.com/", kDefaultBucketName, kPerm, 13},
      {"https://foo.com:8081/", kDefaultBucketName, kPerm, 19},
      {"http://bar.com/", kDefaultBucketName, kPerm, 5},
      {"https://bar.com/", kDefaultBucketName, kPerm, 7},
      {"http://baz.com/", kDefaultBucketName, kPerm, 30},
      {"http://foo.com/", kDefaultBucketName, kTemp, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  SetPersistentHostQuota("foo.com", 100);
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20 + 13 + 19);
  EXPECT_EQ(result.quota, 100);
}

TEST_F(QuotaManagerImplTest, GetPersistentUsage_WithModify) {
  GetUsage_WithModifyTestBody(kPerm);
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_WithAdditionalTasks) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kPerm, 20},
      {"http://bar.com/", kDefaultBucketName, kPerm, 13},
      {"http://foo.com/", kDefaultBucketName, kTemp, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);
  SetPersistentHostQuota("foo.com", 100);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);
  EXPECT_EQ(result.quota, 100);

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kPerm);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 20);
  EXPECT_EQ(additional_callback_count(), 2);
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_NukeManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kPerm, 20},
      {"http://bar.com/", kDefaultBucketName, kPerm, 13},
      {"http://foo.com/", kDefaultBucketName, kTemp, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);
  SetPersistentHostQuota("foo.com", 100);

  set_additional_callback_count(0);

  // Async GetUsageAndQuota call to test manager reset during call.
  blink::mojom::QuotaStatusCode result_status;
  quota_manager_impl()->GetUsageAndQuotaForWebApps(
      ToStorageKey("http://foo.com/"), kPerm,
      base::BindLambdaForTesting(
          [&](QuotaStatusCode status, int64_t usage, int64_t quota) {
            result_status = status;
          }));
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kPerm);

  // Nuke before waiting for callbacks.
  set_quota_manager_impl(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, result_status);
}

TEST_F(QuotaManagerImplTest, GetUsage_Simple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 1},
      {"http://foo.com:1/", kDefaultBucketName, kPerm, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 300},
      {"https://buz.com/", kDefaultBucketName, kTemp, 4000},
      {"http://buz.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com:1/", kDefaultBucketName, kPerm, 600000},
      {"http://foo.com/", kDefaultBucketName, kTemp, 7000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  auto global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(300 + 4000 + 50000 + 7000000, global_usage_result.usage);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(usage(), 1 + 20);

  GetHostUsageWithBreakdown("buz.com", kTemp);
  EXPECT_EQ(usage(), 4000 + 50000);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithModification) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 1},
      {"http://foo.com:1/", kDefaultBucketName, kPerm, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 300},
      {"https://buz.com/", kDefaultBucketName, kTemp, 4000},
      {"http://buz.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com:1/", kDefaultBucketName, kPerm, 600000},
      {"http://foo.com/", kDefaultBucketName, kTemp, 7000000},
  };

  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), kPerm,
                                    80000000);

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), kTemp, 1);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  GetHostUsageWithBreakdown("buz.com", kTemp);
  EXPECT_EQ(usage(), 4000 + 50000);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://buz.com/"), kTemp,
                                    900000000);

  GetHostUsageWithBreakdown("buz.com", kTemp);
  EXPECT_EQ(usage(), 4000 + 50000 + 900000000);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithDeleteBucket) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  int64_t predelete_host_pers = usage();

  auto bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  EXPECT_TRUE(bucket.ok());

  auto status = DeleteBucketData(bucket->ToBucketLocator(),
                                 {QuotaClientType::kFileSystem});
  EXPECT_EQ(status, QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp - 1);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, GetStorageCapacity) {
  auto storage_capacity = GetStorageCapacity();
  EXPECT_GE(storage_capacity.total_space, 0);
  EXPECT_GE(storage_capacity.available_space, 0);
}

TEST_F(QuotaManagerImplTest, EvictBucketData) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", "logs", kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", "logs", kTemp, 6000},
      {"http://foo.com/", kDefaultBucketName, kPerm, 700},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  int64_t predelete_host_pers = usage();

  for (const ClientBucketData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  // Default bucket eviction.
  auto bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  ASSERT_EQ(EvictBucketData(bucket->ToBucketLocator()), QuotaStatusCode::kOk);

  bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_FALSE(bucket.ok());
  ASSERT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), global_usage_result.usage);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_host_tmp - (1 + 50000), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_host_pers, usage());

  // Non default bucket eviction.
  bucket = GetBucket(ToStorageKey("http://foo.com:1"), "logs", kTemp);
  ASSERT_TRUE(bucket.ok());

  ASSERT_EQ(EvictBucketData(bucket->ToBucketLocator()), QuotaStatusCode::kOk);

  bucket = GetBucket(ToStorageKey("http://foo.com:1"), "logs", kTemp);
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 50000 + 6000),
            global_usage_result.usage);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_host_tmp - (1 + 20 + 50000 + 6000), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, EvictBucketDataHistogram) {
  base::HistogramTester histograms;
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(client, kData);

  GetGlobalUsage(kTemp);

  auto bucket =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  ASSERT_EQ(EvictBucketData(bucket->ToBucketLocator()), QuotaStatusCode::kOk);

  // Ensure use count and time since access are recorded.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 1);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 0, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram, 1);

  // Change the use count.
  quota_manager_impl()->NotifyStorageAccessed(ToStorageKey("http://bar.com/"),
                                              kTemp, base::Time::Now());
  task_environment_.RunUntilIdle();

  GetGlobalUsage(kTemp);

  bucket = GetBucket(ToStorageKey("http://bar.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  ASSERT_EQ(EvictBucketData(bucket->ToBucketLocator()), QuotaStatusCode::kOk);

  // The new use count should be logged.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 2);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 1, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram, 2);
}

TEST_F(QuotaManagerImplTest, EvictBucketDataWithDeletionError) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const int kNumberOfTemporaryBuckets = 3;
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, (1 + 20 + 4000));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ((1 + 20), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(300, usage());

  for (const ClientBucketData& data : kData)
    NotifyStorageAccessed(ToStorageKey(data.origin), data.type);
  task_environment_.RunUntilIdle();

  auto bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());
  client->AddBucketToErrorSet(bucket->ToBucketLocator());

  for (int i = 0; i < QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted + 1;
       ++i) {
    ASSERT_EQ(EvictBucketData(bucket->ToBucketLocator()),
              QuotaStatusCode::kErrorInvalidModification);
  }

  // The default bucket for "http://foo.com/" should still be in the database.
  bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  EXPECT_TRUE(bucket.ok());

  for (size_t i = 0; i < kNumberOfTemporaryBuckets - 1; ++i) {
    GetEvictionBucket(kTemp);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(eviction_bucket().has_value());
    // "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"),
              eviction_bucket()->storage_key.origin().GetURL().spec());
    DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  }

  // Now the LRU list must be empty.
  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_bucket().has_value());

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 1);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(1, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(300, usage());
}

TEST_F(QuotaManagerImplTest, GetEvictionRoundInfo) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://unlimited/", kDefaultBucketName, kTemp, 4000},
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetEvictionRoundInfo();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(21, usage());
  EXPECT_EQ(kPoolSize, settings().pool_size);
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataNoClients) {
  EXPECT_EQ(DeleteHostData(std::string(), kTemp), QuotaStatusCode::kOk);
}

TEST_F(QuotaManagerImplTest, DeleteHostDataSimple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  int64_t predelete_host_pers = usage();

  EXPECT_EQ(DeleteHostData(std::string(), kTemp), QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_host_pers, usage());

  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - 1, global_usage_result.usage);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultiple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 6000},
      {"http://foo.com/", kDefaultBucketName, kPerm, 700},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  const int64_t predelete_bar_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  const int64_t predelete_bar_pers = usage();

  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("bar.com", kTemp), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);

  const BucketTableEntries& entries = DumpBucketTable();
  for (const auto& entry : entries) {
    if (entry.type != kTemp)
      continue;

    EXPECT_NE(std::string("http://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://foo.com:1/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("https://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              entry.storage_key.origin().GetURL().spec());
  }

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage,
            predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - (1 + 20 + 50000 + 6000 + 80), usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultipleClientsDifferentTypes) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 1},
      {"http://foo.com:1/", kDefaultBucketName, kPerm, 10},
      {"http://foo.com/", kDefaultBucketName, kTemp, 100},
      {"http://bar.com/", kDefaultBucketName, kPerm, 1000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 100000},
      {"https://foo.com/", kDefaultBucketName, kTemp, 1000000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 10000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  const int64_t predelete_bar_tmp = usage();

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, (1000 + 10 + 1));

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ((10 + 1), usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  EXPECT_EQ(1000, usage());

  EXPECT_EQ(DeleteHostData("foo.com", kPerm), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("bar.com", kPerm), QuotaStatusCode::kOk);

  const BucketTableEntries& entries = DumpBucketTable();
  for (const auto& entry : entries) {
    if (entry.type != kPerm)
      continue;

    EXPECT_NE(std::string("http://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://foo.com:1/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("https://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              entry.storage_key.origin().GetURL().spec());
  }

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp, usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  EXPECT_EQ(predelete_bar_tmp, usage());

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, 0);

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  EXPECT_EQ(0, usage());
}

TEST_F(QuotaManagerImplTest, DeleteBucketNoClients) {
  auto bucket = CreateBucketForTesting(ToStorageKey("http://foo.com"),
                                       kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  EXPECT_EQ(DeleteBucketData(bucket->ToBucketLocator(), AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
}

TEST_F(QuotaManagerImplTest, DeleteBucketDataMultiple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kPerm, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 6000},
      {"http://foo.com/", kDefaultBucketName, kPerm, 700},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto foo_temp_bucket =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(foo_temp_bucket.ok());

  auto bar_temp_bucket =
      GetBucket(ToStorageKey("http://bar.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bar_temp_bucket.ok());

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  const int64_t predelete_bar_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  const int64_t predelete_bar_pers = usage();

  for (const ClientBucketData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DeleteBucketData(foo_temp_bucket->ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteBucketData(bar_temp_bucket->ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);

  QuotaErrorOr<BucketInfo> bucket;
  bucket = GetBucket(foo_temp_bucket->storage_key, foo_temp_bucket->name,
                     foo_temp_bucket->type);
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  bucket = GetBucket(bar_temp_bucket->storage_key, bar_temp_bucket->name,
                     bar_temp_bucket->type);
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage,
            predelete_global_tmp - (1 + 4000 + 50000 + 9));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - (1 + 50000), usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerImplTest, DeleteBucketDataMultipleClientsDifferentTypes) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kPerm, 1},
      {"http://foo.com:1/", kDefaultBucketName, kPerm, 10},
      {"http://foo.com/", kDefaultBucketName, kTemp, 100},
      {"http://bar.com/", kDefaultBucketName, kPerm, 1000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 100000},
      {"https://foo.com/", kDefaultBucketName, kTemp, 1000000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 10000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto foo_perm_bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kPerm);
  ASSERT_TRUE(foo_perm_bucket.ok());

  auto bar_perm_bucket =
      GetBucket(ToStorageKey("http://bar.com/"), kDefaultBucketName, kPerm);
  ASSERT_TRUE(bar_perm_bucket.ok());

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  const int64_t predelete_bar_tmp = usage();

  global_usage_result = GetGlobalUsage(kPerm);
  const int64_t predelete_global_pers = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kPerm);
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  const int64_t predelete_bar_pers = usage();

  for (const ClientBucketData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DeleteBucketData(foo_perm_bucket->ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteBucketData(bar_perm_bucket->ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);

  QuotaErrorOr<BucketInfo> bucket;
  bucket = GetBucket(foo_perm_bucket->storage_key, foo_perm_bucket->name,
                     foo_perm_bucket->type);
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  bucket = GetBucket(bar_perm_bucket->storage_key, bar_perm_bucket->name,
                     bar_perm_bucket->type);
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp, usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  EXPECT_EQ(predelete_bar_tmp, usage());

  global_usage_result = GetGlobalUsage(kPerm);
  EXPECT_EQ(global_usage_result.usage, predelete_global_pers - (1 + 1000));

  GetHostUsageWithBreakdown("foo.com", kPerm);
  EXPECT_EQ(predelete_foo_pers - 1, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  EXPECT_EQ(predelete_bar_pers - 1000, usage());
}

TEST_F(QuotaManagerImplTest, FindAndDeleteBucketData) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto foo_bucket =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(foo_bucket.ok());

  auto bar_bucket =
      GetBucket(ToStorageKey("http://bar.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bar_bucket.ok());

  // Check usage data before deletion.
  auto global_usage_result = GetGlobalUsage(kTemp);
  ASSERT_EQ((1 + 9 + 4000 + 50000), global_usage_result.usage);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  GetHostUsageWithBreakdown("foo.com", kTemp);
  ASSERT_EQ((1 + 50000), usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  ASSERT_EQ((9 + 4000), usage());

  // Delete bucket for "http://foo.com/".
  EXPECT_EQ(FindAndDeleteBucketData(foo_bucket->storage_key, foo_bucket->name),
            QuotaStatusCode::kOk);

  auto bucket =
      GetBucket(foo_bucket->storage_key, foo_bucket->name, foo_bucket->type);
  ASSERT_FALSE(bucket.ok());
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp - (1 + 50000));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(0, usage());

  // Delete bucket for "http://bar.com/".
  EXPECT_EQ(FindAndDeleteBucketData(bar_bucket->storage_key, bar_bucket->name),
            QuotaStatusCode::kOk);

  bucket =
      GetBucket(bar_bucket->storage_key, bar_bucket->name, bar_bucket->type);
  ASSERT_FALSE(bucket.ok());
  EXPECT_EQ(bucket.error(), QuotaError::kNotFound);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);

  GetHostUsageWithBreakdown("bar.com", kTemp);
  EXPECT_EQ(0, usage());
}

TEST_F(QuotaManagerImplTest, FindAndDeleteBucketDataWithDBError) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 123},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});

  auto quota_db = std::make_unique<MockQuotaDatabase>(
      data_dir_.GetPath().AppendASCII("QuotaManager"));
  MockQuotaDatabase* mock_database = quota_db.get();
  SetQuotaDatabase(std::move(quota_db));

  RegisterClientBucketData(fs_client, kData);

  // Check usage data before deletion.
  GetHostUsageWithBreakdown("foo.com", kTemp);
  ASSERT_EQ(123, usage());

  EXPECT_CALL(*mock_database, DeleteBucketInfo)
      .Times(1)
      .WillOnce(testing::Return(QuotaError::kDatabaseError));

  // Trying to delete bucket for "http://foo.com/" should return error.
  EXPECT_EQ(FindAndDeleteBucketData(ToStorageKey("http://foo.com"),
                                    kDefaultBucketName),
            QuotaStatusCode::kErrorInvalidModification);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);

  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(0, usage());
}

TEST_F(QuotaManagerImplTest, NotifyAndLRUBucket) {
  static const ClientBucketData kData[] = {
      {"http://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://a.com:1/", kDefaultBucketName, kTemp, 0},
      {"http://b.com/", kDefaultBucketName, kPerm, 0},
      {"http://c.com/", kDefaultBucketName, kTemp, 0},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  quota_manager_impl()->NotifyStorageAccessed(ToStorageKey("http://b.com/"),
                                              kPerm, base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(ToStorageKey("http://a.com/"),
                                              kTemp, base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(ToStorageKey("http://c.com/"),
                                              kTemp, base::Time::Now());
  task_environment_.RunUntilIdle();

  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com:1/",
            eviction_bucket()->storage_key.origin().GetURL().spec());

  DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/",
            eviction_bucket()->storage_key.origin().GetURL().spec());

  DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://c.com/",
            eviction_bucket()->storage_key.origin().GetURL().spec());
}

TEST_F(QuotaManagerImplTest, GetLRUBucket) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  auto bucket =
      CreateBucketForTesting(storage_key_a, kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());
  BucketInfo bucket_a = bucket.value();

  bucket = CreateBucketForTesting(storage_key_b, kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());
  BucketInfo bucket_b = bucket.value();

  // Persistent bucket.
  bucket = CreateBucketForTesting(storage_key_c, kDefaultBucketName, kPerm);
  ASSERT_TRUE(bucket.ok());
  BucketInfo bucket_c = bucket.value();

  NotifyBucketAccessed(bucket_a.id);
  NotifyBucketAccessed(bucket_b.id);
  NotifyBucketAccessed(bucket_c.id);

  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(bucket_a.ToBucketLocator(), eviction_bucket());

  // Notify that the `bucket_a` is accessed.
  NotifyBucketAccessed(bucket_a.id);
  GetEvictionBucket(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(bucket_b.ToBucketLocator(), eviction_bucket());

  // Notify that the `bucket_b` is accessed while GetEvictionBucket is running.
  GetEvictionBucket(kTemp);
  NotifyBucketAccessed(bucket_b.id);
  task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned storage key, so we will
  // see empty result here.
  EXPECT_FALSE(eviction_bucket().has_value());
}

TEST_F(QuotaManagerImplTest, GetBucketsModifiedBetween) {
  static const ClientBucketData kData[] = {
      {"http://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://a.com:1/", kDefaultBucketName, kTemp, 0},
      {"https://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://b.com/", kDefaultBucketName, kPerm, 0},  // persistent
      {"http://c.com/", kDefaultBucketName, kTemp, 0},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(client, kData);

  auto buckets =
      GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_EQ(4U, buckets.size());

  base::Time time1 = client->IncrementMockTime();
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com:1/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://b.com/"), kPerm, 10);
  base::Time time2 = client->IncrementMockTime();
  client->ModifyStorageKeyAndNotify(ToStorageKey("https://a.com/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://c.com/"), kTemp, 10);
  base::Time time3 = client->IncrementMockTime();

  // Database call to ensure modification calls have completed.
  GetBucket(ToStorageKey("http://a.com"), kDefaultBucketName, kTemp);

  buckets = GetBucketsModifiedBetween(kTemp, time1, base::Time::Max());
  EXPECT_THAT(buckets, testing::UnorderedElementsAre(
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://a.com")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://a.com:1")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("https://a.com")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://c.com"))));

  buckets = GetBucketsModifiedBetween(kTemp, time2, base::Time::Max());
  EXPECT_EQ(2U, buckets.size());

  buckets = GetBucketsModifiedBetween(kTemp, time3, base::Time::Max());
  EXPECT_TRUE(buckets.empty());

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com/"), kTemp, 10);

  // Database call to ensure modification calls have completed.
  GetBucket(ToStorageKey("http://a.com"), kDefaultBucketName, kTemp);

  buckets = GetBucketsModifiedBetween(kTemp, time3, base::Time::Max());
  EXPECT_THAT(buckets,
              testing::UnorderedElementsAre(testing::Field(
                  &BucketLocator::storage_key, ToStorageKey("http://a.com/"))));
}

TEST_F(QuotaManagerImplTest, GetBucketsModifiedBetweenWithDatabaseError) {
  disable_database_bootstrap(true);
  OpenDatabase();

  // Disable quota database for database error behavior.
  disable_quota_database(true);

  auto buckets =
      GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());

  // Return empty set when error is encountered.
  EXPECT_TRUE(buckets.empty());
}

TEST_F(QuotaManagerImplTest, DumpQuotaTable) {
  SetPersistentHostQuota("example1.com", 1);
  SetPersistentHostQuota("example2.com", 20);
  SetPersistentHostQuota("example3.com", 300);
  task_environment_.RunUntilIdle();

  const QuotaTableEntries& entries = DumpQuotaTable();
  EXPECT_THAT(
      entries,
      testing::UnorderedElementsAre(
          QuotaTableEntry{.host = "example1.com", .type = kPerm, .quota = 1},
          QuotaTableEntry{.host = "example2.com", .type = kPerm, .quota = 20},
          QuotaTableEntry{
              .host = "example3.com", .type = kPerm, .quota = 300}));
}

TEST_F(QuotaManagerImplTest, DumpBucketTable) {
  const StorageKey kStorageKey = ToStorageKey("http://example.com/");
  CreateBucketForTesting(kStorageKey, kDefaultBucketName, kTemp);
  CreateBucketForTesting(kStorageKey, kDefaultBucketName, kPerm);

  quota_manager_impl()->NotifyStorageAccessed(kStorageKey, kTemp,
                                              base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(kStorageKey, kPerm,
                                              base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(kStorageKey, kPerm,
                                              base::Time::Now());
  task_environment_.RunUntilIdle();

  const BucketTableEntries& entries = DumpBucketTable();
  EXPECT_THAT(entries, testing::UnorderedElementsAre(
                           MatchesBucketTableEntry(kStorageKey, kTemp, 1),
                           MatchesBucketTableEntry(kStorageKey, kPerm, 2)));
}

TEST_F(QuotaManagerImplTest, QuotaForEmptyHost) {
  EXPECT_EQ(GetPersistentHostQuota(std::string()), 0);

  base::test::TestFuture<QuotaStatusCode, int64_t> future;
  quota_manager_impl()->SetPersistentHostQuota(std::string(), 10,
                                               future.GetCallback());
  EXPECT_EQ(future.Get<0>(), QuotaStatusCode::kErrorNotSupported);
}

TEST_F(QuotaManagerImplTest, DeleteSpecificClientTypeSingleBucket) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 2},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData4[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* idb_client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(sw_client, kData2);
  RegisterClientBucketData(db_client, kData3);
  RegisterClientBucketData(idb_client, kData4);

  auto foo_bucket =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(foo_bucket.ok());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  DeleteBucketData(foo_bucket->ToBucketLocator(),
                   {QuotaClientType::kFileSystem});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteBucketData(foo_bucket->ToBucketLocator(),
                   {QuotaClientType::kServiceWorkerCache});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteBucketData(foo_bucket->ToBucketLocator(), {QuotaClientType::kDatabase});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteBucketData(foo_bucket->ToBucketLocator(),
                   {QuotaClientType::kIndexedDatabase});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, DeleteMultipleClientTypesSingleBucket) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 2},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData4[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* idb_client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(sw_client, kData2);
  RegisterClientBucketData(db_client, kData3);
  RegisterClientBucketData(idb_client, kData4);

  auto foo_bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(foo_bucket.ok());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  const int64_t predelete_foo_tmp = usage();

  DeleteBucketData(foo_bucket->ToBucketLocator(),
                   {QuotaClientType::kFileSystem, QuotaClientType::kDatabase});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 4 - 1, usage());

  DeleteBucketData(foo_bucket->ToBucketLocator(),
                   {QuotaClientType::kServiceWorkerCache,
                    QuotaClientType::kIndexedDatabase});
  GetHostUsageWithBreakdown("foo.com", kTemp);
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManagerImpl(true);

  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com/", kDefaultBucketName, kPerm, 80},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kPerm});
  RegisterClientBucketData(fs_client, kData);

  // Query global usage to warmup the usage tracker caching.
  GetGlobalUsage(kTemp);
  GetGlobalUsage(kPerm);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 80);
  EXPECT_EQ(result.quota, 0);

  const int kPoolSize = 1000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, INT64_C(0));

  auto storage_capacity = GetStorageCapacity();
  EXPECT_EQ(storage_capacity.total_space, kPoolSize);
  EXPECT_EQ(storage_capacity.available_space, kPoolSize - 80 - 10);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_GE(result.quota, kPerHostQuota);

  mock_special_storage_policy()->AddUnlimited(GURL("http://foo.com/"));
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 80);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_SessionOnly) {
  const StorageKey kEpheremalStorageKey = ToStorageKey("http://ephemeral/");
  mock_special_storage_policy()->AddSessionOnly(
      kEpheremalStorageKey.origin().GetURL());

  auto result = GetUsageAndQuotaForWebApps(kEpheremalStorageKey, kTemp);
  EXPECT_EQ(quota_manager_impl()->settings().session_only_per_host_quota,
            result.quota);

  result = GetUsageAndQuotaForWebApps(kEpheremalStorageKey, kPerm);
  EXPECT_EQ(0, result.quota);
}

TEST_F(QuotaManagerImplTest, MaybeRunStoragePressureCallback) {
  bool callback_ran = false;
  auto cb = base::BindRepeating(
      [](bool* callback_ran, StorageKey storage_key) { *callback_ran = true; },
      &callback_ran);

  SetStoragePressureCallback(std::move(cb));

  int64_t kGBytes = QuotaManagerImpl::kMBytes * 1024;
  MaybeRunStoragePressureCallback(StorageKey(), 100 * kGBytes, 2 * kGBytes);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_ran);

  MaybeRunStoragePressureCallback(StorageKey(), 100 * kGBytes, kGBytes);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran);
}

TEST_F(QuotaManagerImplTest, OverrideQuotaForStorageKey) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle = GetQuotaOverrideHandle();

  base::RunLoop run_loop;
  handle->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, 5000);
}

TEST_F(QuotaManagerImplTest, OverrideQuotaForStorageKey_Disable) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle1 = GetQuotaOverrideHandle();
  std::unique_ptr<QuotaOverrideHandle> handle2 = GetQuotaOverrideHandle();

  base::RunLoop run_loop1;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop1.Quit(); }));
  run_loop1.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 5000);

  base::RunLoop run_loop2;
  handle2->OverrideQuotaForStorageKey(
      storage_key, 9000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 9000);

  base::RunLoop run_loop3;
  handle2->OverrideQuotaForStorageKey(
      storage_key, absl::nullopt,
      base::BindLambdaForTesting([&]() { run_loop3.Quit(); }));
  run_loop3.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, kDefaultPerHostQuota);
}

TEST_F(QuotaManagerImplTest, WithdrawQuotaOverride) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle1 = GetQuotaOverrideHandle();
  std::unique_ptr<QuotaOverrideHandle> handle2 = GetQuotaOverrideHandle();

  base::RunLoop run_loop1;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop1.Quit(); }));
  run_loop1.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 5000);

  base::RunLoop run_loop2;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 8000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 8000);

  // Quota should remain overridden if only one of the two handles withdraws
  // it's overrides
  handle2.reset();
  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 8000);

  handle1.reset();
  task_environment_.RunUntilIdle();
  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, kDefaultPerHostQuota);
}

TEST_F(QuotaManagerImplTest, QuotaChangeEvent_LargePartitionPressure) {
  scoped_feature_list_.InitAndEnableFeature(features::kStoragePressureEvent);
  bool quota_change_dispatched = false;

  SetQuotaChangeCallback(
      base::BindLambdaForTesting([&] { quota_change_dispatched = true; }));
  SetGetVolumeInfoFn([](const base::FilePath&) -> std::tuple<int64_t, int64_t> {
    int64_t total = kGigabytes * 100;
    int64_t available = kGigabytes * 2;
    return std::make_tuple(total, available);
  });
  GetStorageCapacity();
  EXPECT_FALSE(quota_change_dispatched);

  SetGetVolumeInfoFn([](const base::FilePath&) -> std::tuple<int64_t, int64_t> {
    int64_t total = kGigabytes * 100;
    int64_t available = QuotaManagerImpl::kMBytes * 512;
    return std::make_tuple(total, available);
  });
  GetStorageCapacity();
  EXPECT_TRUE(quota_change_dispatched);
}

TEST_F(QuotaManagerImplTest, QuotaChangeEvent_SmallPartitionPressure) {
  scoped_feature_list_.InitAndEnableFeature(features::kStoragePressureEvent);
  bool quota_change_dispatched = false;

  SetQuotaChangeCallback(
      base::BindLambdaForTesting([&] { quota_change_dispatched = true; }));
  SetGetVolumeInfoFn([](const base::FilePath&) -> std::tuple<int64_t, int64_t> {
    int64_t total = kGigabytes * 10;
    int64_t available = total * 2;
    return std::make_tuple(total, available);
  });
  GetStorageCapacity();
  EXPECT_FALSE(quota_change_dispatched);

  SetGetVolumeInfoFn([](const base::FilePath&) -> std::tuple<int64_t, int64_t> {
    // DetermineStoragePressure flow will trigger the storage pressure flow
    // when available disk space is below 5% (+/- 0.25%) of total disk space.
    // Available is 2% here to guarantee that it falls below the threshold.
    int64_t total = kGigabytes * 10;
    int64_t available = total * 0.02;
    return std::make_tuple(total, available);
  });
  GetStorageCapacity();
  EXPECT_TRUE(quota_change_dispatched);
}

TEST_F(QuotaManagerImplTest, DeleteBucketData_QuotaManagerDeletedImmediately) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  QuotaErrorOr<BucketInfo> bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  base::test::TestFuture<QuotaStatusCode> delete_bucket_data_future;
  quota_manager_impl_->DeleteBucketData(
      bucket->ToBucketLocator(), {QuotaClientType::kIndexedDatabase},
      delete_bucket_data_future.GetCallback());
  quota_manager_impl_.reset();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, delete_bucket_data_future.Get());
}

TEST_F(QuotaManagerImplTest, DeleteBucketData_CallbackDeletesQuotaManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  QuotaErrorOr<BucketInfo> bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  base::RunLoop run_loop;
  QuotaStatusCode delete_bucket_data_result = QuotaStatusCode::kUnknown;
  quota_manager_impl_->DeleteBucketData(
      bucket->ToBucketLocator(), {QuotaClientType::kIndexedDatabase},
      base::BindLambdaForTesting([&](QuotaStatusCode status_code) {
        quota_manager_impl_.reset();
        delete_bucket_data_result = status_code;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(QuotaStatusCode::kOk, delete_bucket_data_result);
}

TEST_F(QuotaManagerImplTest, DeleteHostData_CallbackDeletesQuotaManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  QuotaErrorOr<BucketInfo> bucket =
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket.ok());

  auto status = DeleteBucketData(bucket->ToBucketLocator(),
                                 {QuotaClientType::kFileSystem});
  EXPECT_EQ(status, QuotaStatusCode::kOk);

  base::RunLoop run_loop;
  QuotaStatusCode delete_host_data_result = QuotaStatusCode::kUnknown;
  quota_manager_impl_->DeleteHostData(
      "foo.com", kTemp,
      base::BindLambdaForTesting([&](QuotaStatusCode status_code) {
        quota_manager_impl_.reset();
        delete_host_data_result = status_code;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(QuotaStatusCode::kOk, delete_host_data_result);
}

}  // namespace storage
