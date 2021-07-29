// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
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
#include "storage/browser/test/mock_special_storage_policy.h"
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

}  // namespace

class QuotaManagerImplTest : public testing::Test {
 protected:
  using QuotaTableEntry = QuotaManagerImpl::QuotaTableEntry;
  using QuotaTableEntries = QuotaManagerImpl::QuotaTableEntries;
  using BucketTableEntries = QuotaManagerImpl::BucketTableEntries;

 public:
  QuotaManagerImplTest() : mock_time_counter_(0) {}

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
      base::span<const MockStorageKeyData> mock_data,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType> storage_types) {
    auto mock_quota_client = std::make_unique<storage::MockQuotaClient>(
        quota_manager_impl_->proxy(), mock_data, client_type);
    MockQuotaClient* mock_quota_client_ptr = mock_quota_client.get();

    mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
    mojo::MakeSelfOwnedReceiver(std::move(mock_quota_client),
                                quota_client.InitWithNewPipeAndPassReceiver());
    quota_manager_impl_->proxy()->RegisterClient(std::move(quota_client),
                                                 client_type, storage_types);
    return mock_quota_client_ptr;
  }

  void GetOrCreateBucket(const StorageKey& storage_key,
                         const std::string& bucket_name) {
    quota_manager_impl_->GetOrCreateBucket(
        storage_key, bucket_name,
        base::BindOnce(&QuotaManagerImplTest::DidGetBucket,
                       weak_factory_.GetWeakPtr()));
  }

  void CreateBucketForTesting(const StorageKey& storage_key,
                              const std::string& bucket_name,
                              blink::mojom::StorageType storage_type) {
    quota_manager_impl_->CreateBucketForTesting(
        storage_key, bucket_name, storage_type,
        base::BindOnce(&QuotaManagerImplTest::DidGetBucket,
                       weak_factory_.GetWeakPtr()));
  }

  void GetBucket(const StorageKey& storage_key,
                 const std::string& bucket_name,
                 blink::mojom::StorageType storage_type) {
    quota_manager_impl_->GetBucket(
        storage_key, bucket_name, storage_type,
        base::BindOnce(&QuotaManagerImplTest::DidGetBucket,
                       weak_factory_.GetWeakPtr()));
  }

  void GetUsageInfo() {
    usage_info_.clear();
    quota_manager_impl_->GetUsageInfo(base::BindOnce(
        &QuotaManagerImplTest::DidGetUsageInfo, weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForWebApps(const StorageKey& storage_key,
                                  StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_impl_->GetUsageAndQuotaForWebApps(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaWithBreakdown(const StorageKey& storage_key,
                                     StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    usage_breakdown_ = nullptr;
    quota_manager_impl_->GetUsageAndQuotaWithBreakdown(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuotaWithBreakdown,
                       weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForStorageClient(const StorageKey& storage_key,
                                        StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_impl_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuota,
                       weak_factory_.GetWeakPtr()));
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

  void GetPersistentHostQuota(const std::string& host) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_impl_->GetPersistentHostQuota(
        host, base::BindOnce(&QuotaManagerImplTest::DidGetHostQuota,
                             weak_factory_.GetWeakPtr()));
  }

  void SetPersistentHostQuota(const std::string& host, int64_t new_quota) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_impl_->SetPersistentHostQuota(
        host, new_quota,
        base::BindOnce(&QuotaManagerImplTest::DidGetHostQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void GetGlobalUsage(StorageType type) {
    usage_ = -1;
    unlimited_usage_ = -1;
    quota_manager_impl_->GetGlobalUsage(
        type, base::BindOnce(&QuotaManagerImplTest::DidGetGlobalUsage,
                             weak_factory_.GetWeakPtr()));
  }

  void GetHostUsageWithBreakdown(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_impl_->GetHostUsageWithBreakdown(
        host, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetHostUsageBreakdown,
                       weak_factory_.GetWeakPtr()));
  }

  void RunAdditionalUsageAndQuotaTask(const StorageKey& storage_key,
                                      StorageType type) {
    quota_manager_impl_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuotaAdditional,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteClientStorageKeyData(mojom::QuotaClient* client,
                                  const StorageKey& storage_key,
                                  StorageType type) {
    DCHECK(client);
    quota_status_ = QuotaStatusCode::kUnknown;
    client->DeleteStorageKeyData(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void EvictStorageKeyData(const StorageKey& storage_key, StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_impl_->EvictStorageKeyData(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteStorageKeyData(const StorageKey& storage_key,
                            StorageType type,
                            QuotaClientTypes quota_client_types) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_impl_->DeleteStorageKeyData(
        storage_key, type, std::move(quota_client_types),
        base::BindOnce(&QuotaManagerImplTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteHostData(const std::string& host,
                      StorageType type,
                      QuotaClientTypes quota_client_types) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_impl_->DeleteHostData(
        host, type, std::move(quota_client_types),
        base::BindOnce(&QuotaManagerImplTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void GetStorageCapacity() {
    available_space_ = -1;
    total_space_ = -1;
    quota_manager_impl_->GetStorageCapacity(
        base::BindOnce(&QuotaManagerImplTest::DidGetStorageCapacity,
                       weak_factory_.GetWeakPtr()));
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

  std::set<StorageKey> GetCachedStorageKeys(StorageType type) {
    return quota_manager_impl_->GetCachedStorageKeys(type);
  }

  void NotifyStorageAccessed(const StorageKey& storage_key, StorageType type) {
    quota_manager_impl_->NotifyStorageAccessed(storage_key, type,
                                               IncrementMockTime());
  }

  void DeleteStorageKeyFromDatabase(const StorageKey& storage_key,
                                    StorageType type) {
    quota_manager_impl_->DeleteStorageKeyFromDatabase(storage_key, type, false);
  }

  void GetEvictionStorageKey(StorageType type) {
    eviction_storage_key_.reset();
    // The quota manager's default eviction policy is to use an LRU eviction
    // policy.
    quota_manager_impl_->GetEvictionStorageKey(
        type, 0,
        base::BindOnce(&QuotaManagerImplTest::DidGetEvictionStorageKey,
                       weak_factory_.GetWeakPtr()));
  }

  void NotifyStorageKeyInUse(const StorageKey& storage_key) {
    quota_manager_impl_->NotifyStorageKeyInUse(storage_key);
  }

  void NotifyStorageKeyNoLongerInUse(const StorageKey& storage_key) {
    quota_manager_impl_->NotifyStorageKeyNoLongerInUse(storage_key);
  }

  void GetStorageKeysModifiedBetween(StorageType type,
                                     base::Time begin,
                                     base::Time end) {
    modified_storage_keys_.clear();
    modified_storage_keys_type_ = StorageType::kUnknown;
    quota_manager_impl_->GetStorageKeysModifiedBetween(
        type, begin, end,
        base::BindOnce(&QuotaManagerImplTest::DidGetModifiedStorageKeys,
                       weak_factory_.GetWeakPtr()));
  }

  void DumpQuotaTable() {
    quota_entries_.clear();
    quota_manager_impl_->DumpQuotaTable(base::BindOnce(
        &QuotaManagerImplTest::DidDumpQuotaTable, weak_factory_.GetWeakPtr()));
  }

  void DumpBucketTable() {
    bucket_entries_.clear();
    quota_manager_impl_->DumpBucketTable(base::BindOnce(
        &QuotaManagerImplTest::DidDumpBucketTable, weak_factory_.GetWeakPtr()));
  }

  void DidGetBucket(QuotaErrorOr<BucketInfo> result) {
    bucket_ = std::move(result);
  }

  void DidGetUsageInfo(UsageInfoEntries entries) {
    usage_info_ = std::move(entries);
  }

  void DidGetUsageAndQuota(QuotaStatusCode status,
                           int64_t usage,
                           int64_t quota) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
  }

  void DidGetUsageAndQuotaWithBreakdown(
      QuotaStatusCode status,
      int64_t usage,
      int64_t quota,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
    usage_breakdown_ = std::move(usage_breakdown);
  }

  void DidGetQuota(QuotaStatusCode status, int64_t quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetStorageCapacity(int64_t total_space, int64_t available_space) {
    total_space_ = total_space;
    available_space_ = available_space;
  }

  void DidGetHostQuota(QuotaStatusCode status, int64_t quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    usage_ = usage;
    unlimited_usage_ = unlimited_usage;
  }

  void DidGetHostUsage(int64_t usage) { usage_ = usage; }

  void StatusCallback(QuotaStatusCode status) {
    ++status_callback_count_;
    quota_status_ = status;
  }

  void DidGetHostUsageBreakdown(
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    usage_ = usage;
    usage_breakdown_ = std::move(usage_breakdown);
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

  void DidGetEvictionStorageKey(const absl::optional<StorageKey>& storage_key) {
    eviction_storage_key_ = storage_key;
    DCHECK(!storage_key.has_value() ||
           !storage_key->origin().GetURL().is_empty());
  }

  void DidGetModifiedStorageKeys(const std::set<StorageKey>& storage_keys,
                                 StorageType type) {
    modified_storage_keys_ = storage_keys;
    modified_storage_keys_type_ = type;
  }

  void DidDumpQuotaTable(const QuotaTableEntries& entries) {
    quota_entries_ = entries;
  }

  void DidDumpBucketTable(const BucketTableEntries& entries) {
    bucket_entries_ = entries;
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

  QuotaStatusCode status() const { return quota_status_; }
  const UsageInfoEntries& usage_info() const { return usage_info_; }
  int64_t usage() const { return usage_; }
  const blink::mojom::UsageBreakdown& usage_breakdown() const {
    return *usage_breakdown_;
  }
  int64_t unlimited_usage() const { return unlimited_usage_; }
  int64_t quota() const { return quota_; }
  int64_t total_space() const { return total_space_; }
  int64_t available_space() const { return available_space_; }
  const absl::optional<StorageKey>& eviction_storage_key() const {
    return eviction_storage_key_;
  }
  const std::set<StorageKey>& modified_storage_keys() const {
    return modified_storage_keys_;
  }
  StorageType modified_storage_keys_type() const {
    return modified_storage_keys_type_;
  }
  const QuotaTableEntries& quota_entries() const { return quota_entries_; }
  const BucketTableEntries& bucket_entries() const { return bucket_entries_; }
  const QuotaSettings& settings() const { return settings_; }
  int status_callback_count() const { return status_callback_count_; }
  void reset_status_callback_count() { status_callback_count_ = 0; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  QuotaErrorOr<BucketInfo> bucket_;

  static std::vector<QuotaClientType> AllClients() {
    // TODO(pwnall): Implement using something other than an empty vector?
    return {};
  }

 private:
  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
  }

  base::ScopedTempDir data_dir_;

  scoped_refptr<QuotaManagerImpl> quota_manager_impl_;
  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  UsageInfoEntries usage_info_;
  int64_t usage_;
  blink::mojom::UsageBreakdownPtr usage_breakdown_;
  int64_t unlimited_usage_;
  int64_t quota_;
  int64_t total_space_;
  int64_t available_space_;
  absl::optional<StorageKey> eviction_storage_key_;
  std::set<StorageKey> modified_storage_keys_;
  StorageType modified_storage_keys_type_;
  QuotaTableEntries quota_entries_;
  BucketTableEntries bucket_entries_;
  QuotaSettings settings_;
  int status_callback_count_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerImplTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerImplTest);
};

TEST_F(QuotaManagerImplTest, GetUsageInfo) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 15},
      {"http://bar.com/", kTemp, 20},
      {"http://bar.com/", kPerm, 50},
  };
  static const MockStorageKeyData kData2[] = {
      {"https://foo.com/", kTemp, 30},
      {"https://foo.com:8081/", kTemp, 35},
      {"http://bar.com/", kPerm, 40},
      {"http://example.com/", kPerm, 40},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageInfo();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(4U, usage_info().size());
  for (const UsageInfo& info : usage_info()) {
    if (info.host == "foo.com" && info.type == kTemp) {
      EXPECT_EQ(10 + 15 + 30 + 35, info.usage);
    } else if (info.host == "bar.com" && info.type == kTemp) {
      EXPECT_EQ(20, info.usage);
    } else if (info.host == "bar.com" && info.type == kPerm) {
      EXPECT_EQ(50 + 40, info.usage);
    } else if (info.host == "example.com" && info.type == kPerm) {
      EXPECT_EQ(40, info.usage);
    } else {
      ADD_FAILURE() << "Unexpected host, type: " << info.host << ", "
                    << static_cast<int>(info.type);
    }
  }
}

TEST_F(QuotaManagerImplTest, GetOrCreateBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  GetOrCreateBucket(storage_key, bucket_name);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(bucket_.ok());

  BucketId created_bucket_id = bucket_.value().id;

  GetOrCreateBucket(storage_key, bucket_name);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bucket_.ok());
  EXPECT_EQ(bucket_.value().id, created_bucket_id);
}

TEST_F(QuotaManagerImplTest, GetBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  CreateBucketForTesting(storage_key, bucket_name, kTemp);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(bucket_.ok());
  BucketInfo created_bucket = bucket_.value();

  GetBucket(storage_key, bucket_name, kTemp);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(bucket_.ok());
  BucketInfo retrieved_bucket = bucket_.value();
  EXPECT_EQ(created_bucket.id, retrieved_bucket.id);

  GetBucket(storage_key, "bucket_b", kTemp);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(bucket_.ok());
  EXPECT_EQ(bucket_.error(), QuotaError::kNotFound);
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Simple) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com/", kPerm, 80},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(0, quota());
  int64_t quota_returned_for_foo = quota();

  GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(quota_returned_for_foo, quota());
}

TEST_F(QuotaManagerImplTest, GetUsage_NoClient) {
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerImplTest, GetUsage_EmptyClient) {
  CreateAndRegisterClient(base::span<MockStorageKeyData>(),
                          QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_MultiStorageKeys) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10}, {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 5},  {"https://bar.com/", kTemp, 7},
      {"http://baz.com/", kTemp, 30}, {"http://foo.com/", kPerm, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  // This time explicitly sets a temporary global quota.
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  // The host's quota should be its full portion of the global quota
  // since there's plenty of diskspace.
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerImplTest, GetUsage_MultipleClients) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://bar.com/", kTemp, 2},
      {"http://bar.com/", kPerm, 4},
      {"http://unlimited/", kPerm, 8},
  };
  static const MockStorageKeyData kData2[] = {
      {"https://foo.com/", kTemp, 128},
      {"http://example.com/", kPerm, 256},
      {"http://unlimited/", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetStorageCapacity();
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  const int64_t kPoolSize = GetAvailableDiskSpaceForTest();
  const int64_t kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 2 + 128 + 512, usage());
  EXPECT_EQ(512, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4 + 8 + 256, usage());
  EXPECT_EQ(8, unlimited_usage());
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_Simple) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com/", kPerm, 80},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 4},
  };
  static const MockStorageKeyData kData3[] = {
      {"http://foo.com/", kTemp, 8},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData3, QuotaClientType::kAppcache,
                          {blink::mojom::StorageType::kTemporary});

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  usage_breakdown_expected.fileSystem = 80;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.appcache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 4 + 8, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 4;
  usage_breakdown_expected.appcache = 8;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.appcache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_NoClient) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultiStorageKeys) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10}, {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 5},  {"https://bar.com/", kTemp, 7},
      {"http://baz.com/", kTemp, 30}, {"http://foo.com/", kPerm, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  usage_breakdown_expected.fileSystem = 10 + 20;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  usage_breakdown_expected.fileSystem = 5 + 7;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultipleClients) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://bar.com/", kTemp, 2},
      {"http://bar.com/", kPerm, 4},
      {"http://unlimited/", kPerm, 8},
  };
  static const MockStorageKeyData kData2[] = {
      {"https://foo.com/", kTemp, 128},
      {"http://example.com/", kPerm, 256},
      {"http://unlimited/", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 128;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  usage_breakdown_expected.fileSystem = 4;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 512;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  usage_breakdown_expected.fileSystem = 8;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

void QuotaManagerImplTest::GetUsage_WithModifyTestBody(const StorageType type) {
  const MockStorageKeyData data[] = {
      {"http://foo.com/", type, 10},
      {"http://foo.com:1/", type, 20},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(data, QuotaClientType::kFileSystem, {type});

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), type, 30);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com:1/"), type,
                                    -5);
  client->AddStorageKeyAndNotify(ToStorageKey("https://foo.com/"), type, 1);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 30 - 5 + 1, usage());
  int foo_usage = usage();

  client->AddStorageKeyAndNotify(ToStorageKey("http://bar.com/"), type, 40);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(40, usage());

  GetGlobalUsage(type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(foo_usage + 40, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsage_WithModify) {
  GetUsage_WithModifyTestBody(kTemp);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 13},
      {"http://foo.com/", kPerm, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 13},
      {"http://foo.com/", kPerm, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);

  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       AllQuotaClientTypes());
  DeleteStorageKeyData(ToStorageKey("http://bar.com/"), kTemp,
                       AllQuotaClientTypes());

  // Nuke before waiting for callbacks.
  set_quota_manager_impl(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const MockStorageKeyData kData[] = {
      {"http://usage1/", kTemp, 1},
      {"http://usage10/", kTemp, 10},
      {"http://usage200/", kTemp, 200},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  // Provided diskspace is not tight, global usage does not affect the
  // quota calculations for an individual storage key, so despite global usage
  // in excess of our poolsize, we still get the nominal quota value.
  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_LE(kMustRemainAvailableForSystem, available_space());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage1/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage200/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(200, usage());
  EXPECT_EQ(kPerHostQuota, quota());  // should be clamped to the nominal quota
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const MockStorageKeyData kData[] = {
      {"http://usage10/", kTemp, 10},
      {"http://usage50/", kTemp, 50},
      {"http://unlimited/", kTemp, 4000},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetStorageCapacity();
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});

  // Test when not overbugdet.
  const int kPerHostQuotaFor1000 = 200;
  SetQuotaSettings(1000, kPerHostQuotaFor1000, kMustRemainAvailableForSystem);
  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(4000, unlimited_usage());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManagerImpl::kNoLimit, quota());

  // Test when overbugdet.
  const int kPerHostQuotaFor100 = 20;
  SetQuotaSettings(100, kPerHostQuotaFor100, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManagerImpl::kNoLimit, quota());

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());
}

TEST_F(QuotaManagerImplTest, StorageKeyInUse) {
  const StorageKey kFooStorageKey = ToStorageKey("http://foo.com/");
  const StorageKey kBarStorageKey = ToStorageKey("http://bar.com/");

  EXPECT_FALSE(quota_manager_impl()->IsStorageKeyInUse(kFooStorageKey));
  quota_manager_impl()->NotifyStorageKeyInUse(kFooStorageKey);  // count of 1
  EXPECT_TRUE(quota_manager_impl()->IsStorageKeyInUse(kFooStorageKey));
  quota_manager_impl()->NotifyStorageKeyInUse(kFooStorageKey);  // count of 2
  EXPECT_TRUE(quota_manager_impl()->IsStorageKeyInUse(kFooStorageKey));
  quota_manager_impl()->NotifyStorageKeyNoLongerInUse(
      kFooStorageKey);  // count of 1
  EXPECT_TRUE(quota_manager_impl()->IsStorageKeyInUse(kFooStorageKey));

  EXPECT_FALSE(quota_manager_impl()->IsStorageKeyInUse(kBarStorageKey));
  quota_manager_impl()->NotifyStorageKeyInUse(kBarStorageKey);
  EXPECT_TRUE(quota_manager_impl()->IsStorageKeyInUse(kBarStorageKey));
  quota_manager_impl()->NotifyStorageKeyNoLongerInUse(kBarStorageKey);
  EXPECT_FALSE(quota_manager_impl()->IsStorageKeyInUse(kBarStorageKey));

  quota_manager_impl()->NotifyStorageKeyNoLongerInUse(kFooStorageKey);
  EXPECT_FALSE(quota_manager_impl()->IsStorageKeyInUse(kFooStorageKey));
}

TEST_F(QuotaManagerImplTest, GetAndSetPerststentHostQuota) {
  CreateAndRegisterClient(base::span<MockStorageKeyData>(),
                          QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(100, quota());

  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", 200);
  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com",
                         QuotaManagerImpl::kPerHostPersistentQuotaLimit);
  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManagerImpl::kPerHostPersistentQuotaLimit, quota());

  // Persistent quota should be capped at the per-host quota limit.
  SetPersistentHostQuota("foo.com",
                         QuotaManagerImpl::kPerHostPersistentQuotaLimit + 100);
  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManagerImpl::kPerHostPersistentQuotaLimit, quota());
}

TEST_F(QuotaManagerImplTest, GetAndSetPersistentUsageAndQuota) {
  GetStorageCapacity();
  CreateAndRegisterClient(base::span<MockStorageKeyData>(),
                          QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(100, quota());

  // The actual space available is given to 'unlimited' storage keys as their
  // quota.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(available_space() + usage(), quota());

  // GetUsageAndQuotaForStorageClient should just return 0 usage and
  // kNoLimit quota.
  GetUsageAndQuotaForStorageClient(ToStorageKey("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManagerImpl::kNoLimit, quota());
}

TEST_F(QuotaManagerImplTest, GetQuotaLowAvailableDiskSpace) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 100000},
      {"http://unlimited/", kTemp, 4000000},
  };

  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;

  // In here, we expect the low available space logic branch
  // to be ignored. Doing so should have QuotaManagerImpl return the same
  // per-host quota as what is set in QuotaSettings, despite being in a state of
  // low available space.
  const int kMustRemainAvailable =
      static_cast<int>(GetAvailableDiskSpaceForTest() - 65536);
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailable);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(100000, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerImplTest, GetSyncableQuota) {
  CreateAndRegisterClient(base::span<MockStorageKeyData>(),
                          QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kSyncable});

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
  GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kSync);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManagerImpl::kSyncableStorageDefaultHostQuota, quota());
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_MultiStorageKeys) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kPerm, 10},  {"http://foo.com:8080/", kPerm, 20},
      {"https://foo.com/", kPerm, 13}, {"https://foo.com:8081/", kPerm, 19},
      {"http://bar.com/", kPerm, 5},   {"https://bar.com/", kPerm, 7},
      {"http://baz.com/", kPerm, 30},  {"http://foo.com/", kTemp, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 13 + 19, usage());
  EXPECT_EQ(100, quota());
}

TEST_F(QuotaManagerImplTest, GetPersistentUsage_WithModify) {
  GetUsage_WithModifyTestBody(kPerm);
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_WithAdditionalTasks) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kPerm, 10},
      {"http://foo.com:8080/", kPerm, 20},
      {"http://bar.com/", kPerm, 13},
      {"http://foo.com/", kTemp, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  SetPersistentHostQuota("foo.com", 100);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(100, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerImplTest, GetPersistentUsageAndQuota_NukeManager) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kPerm, 10},
      {"http://foo.com:8080/", kPerm, 20},
      {"http://bar.com/", kPerm, 13},
      {"http://foo.com/", kTemp, 40},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  SetPersistentHostQuota("foo.com", 100);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kPerm);

  // Nuke before waiting for callbacks.
  set_quota_manager_impl(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerImplTest, GetUsage_Simple) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kPerm, 1},       {"http://foo.com:1/", kPerm, 20},
      {"http://bar.com/", kTemp, 300},     {"https://buz.com/", kTemp, 4000},
      {"http://buz.com/", kTemp, 50000},   {"http://bar.com:1/", kPerm, 600000},
      {"http://foo.com/", kTemp, 7000000},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20);

  GetHostUsageWithBreakdown("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithModification) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kPerm, 1},       {"http://foo.com:1/", kPerm, 20},
      {"http://bar.com/", kTemp, 300},     {"https://buz.com/", kTemp, 4000},
      {"http://buz.com/", kTemp, 50000},   {"http://bar.com:1/", kPerm, 600000},
      {"http://foo.com/", kTemp, 7000000},
  };

  MockQuotaClient* client =
      CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                              {blink::mojom::StorageType::kTemporary,
                               blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), kPerm,
                                    80000000);

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://foo.com/"), kTemp, 1);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsageWithBreakdown("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://buz.com/"), kTemp,
                                    900000000);

  GetHostUsageWithBreakdown("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000 + 900000000);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithDeleteStorageKey) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://bar.com/", kTemp, 4000},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                              {blink::mojom::StorageType::kTemporary,
                               blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteClientStorageKeyData(client, ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, GetStorageCapacity) {
  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_LE(0, total_space());
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerImplTest, EvictStorageKeyData) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://bar.com/", kTemp, 4000},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 50000}, {"http://foo.com:1/", kTemp, 6000},
      {"http://foo.com/", kPerm, 700},   {"https://foo.com/", kTemp, 80},
      {"http://bar.com/", kTemp, 9},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (const MockStorageKeyData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const MockStorageKeyData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  EvictStorageKeyData(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : bucket_entries()) {
    if (entry.type == kTemp)
      EXPECT_NE(std::string("http://foo.com/"),
                entry.storage_key.origin().GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - (1 + 50000), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, EvictStorageKeyDataHistogram) {
  const StorageKey kStorageKey = ToStorageKey("http://foo.com/");
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 1},
  };

  base::HistogramTester histograms;
  MockQuotaClient* client =
      CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                              {blink::mojom::StorageType::kTemporary});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();

  EvictStorageKeyData(kStorageKey, kTemp);
  task_environment_.RunUntilIdle();

  // Ensure use count and time since access are recorded.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedOriginAccessedCountHistogram, 1);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedOriginAccessedCountHistogram, 0, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedOriginDaysSinceAccessHistogram, 1);

  client->AddStorageKeyAndNotify(kStorageKey, kTemp, 100);

  // Change the use count of the storage key.
  quota_manager_impl()->NotifyStorageAccessed(kStorageKey, kTemp,
                                              base::Time::Now());
  task_environment_.RunUntilIdle();

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();

  EvictStorageKeyData(kStorageKey, kTemp);
  task_environment_.RunUntilIdle();

  // The new use count should be logged.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedOriginAccessedCountHistogram, 2);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedOriginAccessedCountHistogram, 1, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedOriginDaysSinceAccessHistogram, 2);
}

TEST_F(QuotaManagerImplTest, EvictStorageKeyDataWithDeletionError) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://bar.com/", kTemp, 4000},
  };
  static const int kNumberOfTemporaryStorageKeys = 3;
  MockQuotaClient* client =
      CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                              {blink::mojom::StorageType::kTemporary,
                               blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (const MockStorageKeyData& data : kData)
    NotifyStorageAccessed(ToStorageKey(data.origin), data.type);
  task_environment_.RunUntilIdle();

  client->AddStorageKeyToErrorSet(ToStorageKey("http://foo.com/"), kTemp);

  for (int i = 0; i < QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted + 1;
       ++i) {
    EvictStorageKeyData(ToStorageKey("http://foo.com/"), kTemp);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(QuotaStatusCode::kErrorInvalidModification, status());
  }

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  bool found_storage_key_in_database = false;
  for (const auto& entry : bucket_entries()) {
    if (entry.type == kTemp &&
        entry.storage_key == ToStorageKey("http://foo.com/")) {
      found_storage_key_in_database = true;
      break;
    }
  }
  // The storage key for "http://foo.com/" should be in the database.
  EXPECT_TRUE(found_storage_key_in_database);

  for (size_t i = 0; i < kNumberOfTemporaryStorageKeys - 1; ++i) {
    GetEvictionStorageKey(kTemp);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(eviction_storage_key().has_value());
    // "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"),
              eviction_storage_key()->origin().GetURL().spec());
    DeleteStorageKeyFromDatabase(*eviction_storage_key(), kTemp);
    task_environment_.RunUntilIdle();
  }

  // Now the LRU list must be empty.
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_storage_key().has_value());

  // Deleting storage keys from the database should not affect the results of
  // the following checks.
  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, GetEvictionRoundInfo) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://unlimited/", kTemp, 4000},
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

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
  DeleteHostData(std::string(), kTemp, AllQuotaClientTypes());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataSimple) {
  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 1},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteHostData(std::string(), kTemp, AllQuotaClientTypes());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());

  DeleteHostData("foo.com", kTemp, AllQuotaClientTypes());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultiple) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://bar.com/", kTemp, 4000},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 50000}, {"http://foo.com:1/", kTemp, 6000},
      {"http://foo.com/", kPerm, 700},   {"https://foo.com/", kTemp, 80},
      {"http://bar.com/", kTemp, 9},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  reset_status_callback_count();
  DeleteHostData("foo.com", kTemp, AllQuotaClientTypes());
  DeleteHostData("bar.com", kTemp, AllQuotaClientTypes());
  DeleteHostData("foo.com", kTemp, AllQuotaClientTypes());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : bucket_entries()) {
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

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9),
            usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 20 + 50000 + 6000 + 80), usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultipleClientsDifferentTypes) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kPerm, 1},
      {"http://foo.com:1/", kPerm, 10},
      {"http://foo.com/", kTemp, 100},
      {"http://bar.com/", kPerm, 1000},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 10000},
      {"http://foo.com:1/", kTemp, 100000},
      {"https://foo.com/", kTemp, 1000000},
      {"http://bar.com/", kTemp, 10000000},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_pers = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  reset_status_callback_count();
  DeleteHostData("foo.com", kPerm, AllQuotaClientTypes());
  DeleteHostData("bar.com", kPerm, AllQuotaClientTypes());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, status_callback_count());

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : bucket_entries()) {
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

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp, usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp, usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_pers - (1 + 10 + 1000), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers - (1 + 10), usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers - 1000, usage());
}

TEST_F(QuotaManagerImplTest, DeleteStorageKeyDataNoClients) {
  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       AllQuotaClientTypes());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
}

// Single-run DeleteStorageKeyData cases must be well covered by
// EvictStorageKeyData tests.
TEST_F(QuotaManagerImplTest, DeleteStorageKeyDataMultiple) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://foo.com:1/", kTemp, 20},
      {"http://foo.com/", kPerm, 300},
      {"http://bar.com/", kTemp, 4000},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 50000}, {"http://foo.com:1/", kTemp, 6000},
      {"http://foo.com/", kPerm, 700},   {"https://foo.com/", kTemp, 80},
      {"http://bar.com/", kTemp, 9},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  for (const MockStorageKeyData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const MockStorageKeyData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  reset_status_callback_count();
  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       AllQuotaClientTypes());
  DeleteStorageKeyData(ToStorageKey("http://bar.com/"), kTemp,
                       AllQuotaClientTypes());
  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       AllQuotaClientTypes());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : bucket_entries()) {
    if (entry.type != kTemp)
      continue;

    EXPECT_NE(std::string("http://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              entry.storage_key.origin().GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 4000 + 50000 + 9), usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 50000), usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerImplTest,
       DeleteStorageKeyDataMultipleClientsDifferentTypes) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kPerm, 1},
      {"http://foo.com:1/", kPerm, 10},
      {"http://foo.com/", kTemp, 100},
      {"http://bar.com/", kPerm, 1000},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 10000},
      {"http://foo.com:1/", kTemp, 100000},
      {"https://foo.com/", kTemp, 1000000},
      {"http://bar.com/", kTemp, 10000000},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});
  CreateAndRegisterClient(kData2, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_pers = usage();

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  for (const MockStorageKeyData& data : kData1) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  for (const MockStorageKeyData& data : kData2) {
    quota_manager_impl()->NotifyStorageAccessed(ToStorageKey(data.origin),
                                                data.type, base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  reset_status_callback_count();
  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kPerm,
                       AllQuotaClientTypes());
  DeleteStorageKeyData(ToStorageKey("http://bar.com/"), kPerm,
                       AllQuotaClientTypes());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, status_callback_count());

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : bucket_entries()) {
    if (entry.type != kPerm)
      continue;

    EXPECT_NE(std::string("http://foo.com/"),
              entry.storage_key.origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              entry.storage_key.origin().GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp, usage());

  GetHostUsageWithBreakdown("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp, usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_pers - (1 + 1000), usage());

  GetHostUsageWithBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers - 1, usage());

  GetHostUsageWithBreakdown("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers - 1000, usage());
}

TEST_F(QuotaManagerImplTest, GetCachedStorageKeys) {
  static const MockStorageKeyData kData[] = {
      {"http://a.com/", kTemp, 1},
      {"http://a.com:1/", kTemp, 20},
      {"http://b.com/", kPerm, 300},
      {"http://c.com/", kTemp, 4000},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  // TODO(kinuko): Be careful when we add cache pruner.

  std::set<StorageKey> storage_keys = GetCachedStorageKeys(kTemp);
  EXPECT_TRUE(storage_keys.empty());

  GetHostUsageWithBreakdown("a.com", kTemp);
  task_environment_.RunUntilIdle();
  storage_keys = GetCachedStorageKeys(kTemp);
  EXPECT_EQ(2U, storage_keys.size());

  GetHostUsageWithBreakdown("b.com", kTemp);
  task_environment_.RunUntilIdle();
  storage_keys = GetCachedStorageKeys(kTemp);
  EXPECT_EQ(2U, storage_keys.size());

  GetHostUsageWithBreakdown("c.com", kTemp);
  task_environment_.RunUntilIdle();
  storage_keys = GetCachedStorageKeys(kTemp);
  EXPECT_EQ(3U, storage_keys.size());

  storage_keys = GetCachedStorageKeys(kPerm);
  EXPECT_TRUE(storage_keys.empty());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  storage_keys = GetCachedStorageKeys(kTemp);
  EXPECT_EQ(3U, storage_keys.size());

  for (const MockStorageKeyData& data : kData) {
    if (data.type == kTemp)
      EXPECT_TRUE(base::Contains(storage_keys, ToStorageKey(data.origin)));
  }
}

TEST_F(QuotaManagerImplTest, NotifyAndLRUStorageKey) {
  static const MockStorageKeyData kData[] = {
      {"http://a.com/", kTemp, 0},  {"http://a.com:1/", kTemp, 0},
      {"https://a.com/", kTemp, 0}, {"http://b.com/", kPerm, 0},  // persistent
      {"http://c.com/", kTemp, 0},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_storage_key().has_value());

  NotifyStorageAccessed(ToStorageKey("http://a.com/"), kTemp);
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_storage_key()->origin().GetURL().spec());

  NotifyStorageAccessed(ToStorageKey("http://b.com/"), kPerm);
  NotifyStorageAccessed(ToStorageKey("https://a.com/"), kTemp);
  NotifyStorageAccessed(ToStorageKey("http://c.com/"), kTemp);
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_storage_key()->origin().GetURL().spec());

  DeleteStorageKeyFromDatabase(*eviction_storage_key(), kTemp);
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("https://a.com/", eviction_storage_key()->origin().GetURL().spec());

  DeleteStorageKeyFromDatabase(*eviction_storage_key(), kTemp);
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://c.com/", eviction_storage_key()->origin().GetURL().spec());
}

TEST_F(QuotaManagerImplTest, GetLRUStorageKeyWithStorageKeyInUse) {
  static const MockStorageKeyData kData[] = {
      {"http://a.com/", kTemp, 0},  {"http://a.com:1/", kTemp, 0},
      {"https://a.com/", kTemp, 0}, {"http://b.com/", kPerm, 0},  // persistent
      {"http://c.com/", kTemp, 0},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_storage_key().has_value());

  NotifyStorageAccessed(ToStorageKey("http://a.com/"), kTemp);
  NotifyStorageAccessed(ToStorageKey("http://b.com/"), kPerm);
  NotifyStorageAccessed(ToStorageKey("https://a.com/"), kTemp);
  NotifyStorageAccessed(ToStorageKey("http://c.com/"), kTemp);

  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToStorageKey("http://a.com/"), *eviction_storage_key());

  // Notify that the storage key for http://a.com is in use.
  NotifyStorageKeyInUse(ToStorageKey("http://a.com/"));
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToStorageKey("https://a.com/"), *eviction_storage_key());

  // Notify that the storage key for https://a.com is in use while
  // GetEvictionStorageKey is running.
  GetEvictionStorageKey(kTemp);
  NotifyStorageKeyInUse(ToStorageKey("https://a.com/"));
  task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned storage key, so we will
  // see empty result here.
  EXPECT_FALSE(eviction_storage_key().has_value());

  // Notify access for http://c.com while GetEvictionStorageKey is running.
  GetEvictionStorageKey(kTemp);
  NotifyStorageAccessed(ToStorageKey("http://c.com/"), kTemp);
  task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned storage key, so we will
  // see empty result here.
  EXPECT_FALSE(eviction_storage_key().has_value());

  NotifyStorageKeyNoLongerInUse(ToStorageKey("http://a.com/"));
  NotifyStorageKeyNoLongerInUse(ToStorageKey("https://a.com/"));
  GetEvictionStorageKey(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToStorageKey("http://a.com/"), *eviction_storage_key());
}

TEST_F(QuotaManagerImplTest, GetStorageKeysModifiedBetween) {
  static const MockStorageKeyData kData[] = {
      {"http://a.com/", kTemp, 0},  {"http://a.com:1/", kTemp, 0},
      {"https://a.com/", kTemp, 0}, {"http://b.com/", kPerm, 0},  // persistent
      {"http://c.com/", kTemp, 0},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                              {blink::mojom::StorageType::kTemporary,
                               blink::mojom::StorageType::kPersistent});

  GetStorageKeysModifiedBetween(kTemp, base::Time(), base::Time::Max());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_storage_keys().empty());
  EXPECT_EQ(modified_storage_keys_type(), kTemp);

  base::Time time1 = client->IncrementMockTime();
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com:1/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://b.com/"), kPerm, 10);
  base::Time time2 = client->IncrementMockTime();
  client->ModifyStorageKeyAndNotify(ToStorageKey("https://a.com/"), kTemp, 10);
  client->ModifyStorageKeyAndNotify(ToStorageKey("http://c.com/"), kTemp, 10);
  base::Time time3 = client->IncrementMockTime();

  GetStorageKeysModifiedBetween(kTemp, time1, base::Time::Max());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(4U, modified_storage_keys().size());
  EXPECT_EQ(modified_storage_keys_type(), kTemp);
  for (const MockStorageKeyData& data : kData) {
    if (data.type == kTemp)
      EXPECT_EQ(1U, modified_storage_keys().count(ToStorageKey(data.origin)));
  }

  GetStorageKeysModifiedBetween(kTemp, time2, base::Time::Max());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2U, modified_storage_keys().size());

  GetStorageKeysModifiedBetween(kTemp, time3, base::Time::Max());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_storage_keys().empty());
  EXPECT_EQ(modified_storage_keys_type(), kTemp);

  client->ModifyStorageKeyAndNotify(ToStorageKey("http://a.com/"), kTemp, 10);

  GetStorageKeysModifiedBetween(kTemp, time3, base::Time::Max());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, modified_storage_keys().size());
  EXPECT_EQ(1U, modified_storage_keys().count(ToStorageKey("http://a.com/")));
  EXPECT_EQ(modified_storage_keys_type(), kTemp);
}

TEST_F(QuotaManagerImplTest, DumpQuotaTable) {
  SetPersistentHostQuota("example1.com", 1);
  SetPersistentHostQuota("example2.com", 20);
  SetPersistentHostQuota("example3.com", 300);
  task_environment_.RunUntilIdle();

  DumpQuotaTable();
  task_environment_.RunUntilIdle();

  const QuotaTableEntry kEntries[] = {
      {.host = "example1.com", .type = kPerm, .quota = 1},
      {.host = "example2.com", .type = kPerm, .quota = 20},
      {.host = "example3.com", .type = kPerm, .quota = 300}};
  std::set<QuotaTableEntry> entries(kEntries, kEntries + base::size(kEntries));

  for (const auto& quota_entry : quota_entries()) {
    SCOPED_TRACE(testing::Message() << "host = " << quota_entry.host << ", "
                                    << "quota = " << quota_entry.quota);
    EXPECT_EQ(1u, entries.erase(quota_entry));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerImplTest, DumpBucketTable) {
  using std::make_pair;

  quota_manager_impl()->NotifyStorageAccessed(
      ToStorageKey("http://example.com/"), kTemp, base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(
      ToStorageKey("http://example.com/"), kPerm, base::Time::Now());
  quota_manager_impl()->NotifyStorageAccessed(
      ToStorageKey("http://example.com/"), kPerm, base::Time::Now());
  task_environment_.RunUntilIdle();

  DumpBucketTable();
  task_environment_.RunUntilIdle();

  using TypedStorageKey = std::pair<GURL, StorageType>;
  using Entry = std::pair<TypedStorageKey, int>;
  const Entry kEntries[] = {
    make_pair(make_pair(GURL("http://example.com/"), kTemp), 1),
    make_pair(make_pair(GURL("http://example.com/"), kPerm), 2),
  };
  std::set<Entry> entries(kEntries, kEntries + base::size(kEntries));

  for (const auto& entry : bucket_entries()) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << entry.storage_key.origin() << ", "
                 << "type = " << static_cast<int>(entry.type) << ", "
                 << "use_count = " << entry.use_count);
    EXPECT_EQ(1u,
              entries.erase(make_pair(
                  make_pair(entry.storage_key.origin().GetURL(), entry.type),
                  entry.use_count)));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerImplTest, QuotaForEmptyHost) {
  GetPersistentHostQuota(std::string());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota(std::string(), 10);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorNotSupported, status());
}

TEST_F(QuotaManagerImplTest, DeleteSpecificClientTypeSingleStorageKey) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 2},
  };
  static const MockStorageKeyData kData3[] = {
      {"http://foo.com/", kTemp, 4},
  };
  static const MockStorageKeyData kData4[] = {
      {"http://foo.com/", kTemp, 8},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData2, QuotaClientType::kAppcache,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData3, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData4, QuotaClientType::kIndexedDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       {QuotaClientType::kFileSystem});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       {QuotaClientType::kAppcache});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       {QuotaClientType::kDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteStorageKeyData(ToStorageKey("http://foo.com/"), kTemp,
                       {QuotaClientType::kIndexedDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, DeleteSpecificClientTypeSingleHost) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com:1111/", kTemp, 1},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com:2222/", kTemp, 2},
  };
  static const MockStorageKeyData kData3[] = {
      {"http://foo.com:3333/", kTemp, 4},
  };
  static const MockStorageKeyData kData4[] = {
      {"http://foo.com:4444/", kTemp, 8},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData2, QuotaClientType::kAppcache,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData3, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData4, QuotaClientType::kIndexedDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp, {QuotaClientType::kFileSystem});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteHostData("foo.com", kTemp, {QuotaClientType::kAppcache});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, {QuotaClientType::kDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, {QuotaClientType::kIndexedDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, DeleteMultipleClientTypesSingleStorageKey) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 1},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com/", kTemp, 2},
  };
  static const MockStorageKeyData kData3[] = {
      {"http://foo.com/", kTemp, 4},
  };
  static const MockStorageKeyData kData4[] = {
      {"http://foo.com/", kTemp, 8},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData2, QuotaClientType::kAppcache,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData3, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData4, QuotaClientType::kIndexedDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteStorageKeyData(
      ToStorageKey("http://foo.com/"), kTemp,
      {QuotaClientType::kFileSystem, QuotaClientType::kDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 1, usage());

  DeleteStorageKeyData(
      ToStorageKey("http://foo.com/"), kTemp,
      {QuotaClientType::kAppcache, QuotaClientType::kIndexedDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, DeleteMultipleClientTypesSingleHost) {
  static const MockStorageKeyData kData1[] = {
      {"http://foo.com:1111/", kTemp, 1},
  };
  static const MockStorageKeyData kData2[] = {
      {"http://foo.com:2222/", kTemp, 2},
  };
  static const MockStorageKeyData kData3[] = {
      {"http://foo.com:3333/", kTemp, 4},
  };
  static const MockStorageKeyData kData4[] = {
      {"http://foo.com:4444/", kTemp, 8},
  };
  CreateAndRegisterClient(kData1, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData2, QuotaClientType::kAppcache,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData3, QuotaClientType::kDatabase,
                          {blink::mojom::StorageType::kTemporary});
  CreateAndRegisterClient(kData4, QuotaClientType::kIndexedDatabase,
                          {blink::mojom::StorageType::kTemporary});

  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp,
                 {QuotaClientType::kFileSystem, QuotaClientType::kAppcache});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData(
      "foo.com", kTemp,
      {QuotaClientType::kDatabase, QuotaClientType::kIndexedDatabase});
  task_environment_.RunUntilIdle();
  GetHostUsageWithBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManagerImpl(true);

  static const MockStorageKeyData kData[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com/", kPerm, 80},
  };
  CreateAndRegisterClient(kData, QuotaClientType::kFileSystem,
                          {blink::mojom::StorageType::kTemporary,
                           blink::mojom::StorageType::kPersistent});

  // Query global usage to warmup the usage tracker caching.
  GetGlobalUsage(kTemp);
  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  const int kPoolSize = 1000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, INT64_C(0));

  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kPoolSize, total_space());
  EXPECT_EQ(kPoolSize - 80 - 10, available_space());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(kPerHostQuota, quota());

  mock_special_storage_policy()->AddUnlimited(GURL("http://foo.com/"));
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(available_space() + usage(), quota());
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_SessionOnly) {
  const StorageKey kEpheremalStorageKey = ToStorageKey("http://ephemeral/");
  mock_special_storage_policy()->AddSessionOnly(
      kEpheremalStorageKey.origin().GetURL());

  GetUsageAndQuotaForWebApps(kEpheremalStorageKey, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(quota_manager_impl()->settings().session_only_per_host_quota,
            quota());

  GetUsageAndQuotaForWebApps(kEpheremalStorageKey, kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());
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

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(5000, quota());
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

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5000, quota());

  base::RunLoop run_loop2;
  handle2->OverrideQuotaForStorageKey(
      storage_key, 9000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(9000, quota());

  base::RunLoop run_loop3;
  handle2->OverrideQuotaForStorageKey(
      storage_key, absl::nullopt,
      base::BindLambdaForTesting([&]() { run_loop3.Quit(); }));
  run_loop3.Run();

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(kDefaultPerHostQuota, quota());
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

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5000, quota());

  base::RunLoop run_loop2;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 8000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8000, quota());

  // Quota should remain overridden if only one of the two handles withdraws
  // it's overrides
  handle2.reset();
  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8000, quota());

  handle1.reset();
  task_environment_.RunUntilIdle();
  GetUsageAndQuotaForWebApps(storage_key, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(kDefaultPerHostQuota, quota());
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
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(quota_change_dispatched);

  SetGetVolumeInfoFn([](const base::FilePath&) -> std::tuple<int64_t, int64_t> {
    int64_t total = kGigabytes * 100;
    int64_t available = QuotaManagerImpl::kMBytes * 512;
    return std::make_tuple(total, available);
  });
  GetStorageCapacity();
  task_environment_.RunUntilIdle();
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
  task_environment_.RunUntilIdle();
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
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(quota_change_dispatched);
}

}  // namespace storage
