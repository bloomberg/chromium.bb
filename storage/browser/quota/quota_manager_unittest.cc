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
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/mock_storage_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;
using storage::QuotaClient;
using storage::QuotaManager;
using storage::UsageInfo;
using storage::UsageInfoEntries;

namespace content {

namespace {

// For shorter names.
const StorageType kTemp = StorageType::kTemporary;
const StorageType kPerm = StorageType::kPersistent;
const StorageType kSync = StorageType::kSyncable;

const int kAllClients = QuotaClient::kAllClientsMask;

// Values in bytes.
const int64_t kAvailableSpaceForApp = 13377331U;
const int64_t kMustRemainAvailableForSystem = kAvailableSpaceForApp / 2;
const int64_t kDefaultPoolSize = 1000;
const int64_t kDefaultPerHostQuota = 200;

const GURL kTestEvictionOrigin = GURL("http://test.eviction.policy/result");

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

}  // namespace

class QuotaManagerTest : public testing::Test {
 protected:
  typedef QuotaManager::QuotaTableEntry QuotaTableEntry;
  typedef QuotaManager::QuotaTableEntries QuotaTableEntries;
  typedef QuotaManager::OriginInfoTableEntries OriginInfoTableEntries;

 public:
  QuotaManagerTest()
      : mock_time_counter_(0),
        weak_factory_(this) {
  }

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    mock_special_storage_policy_ = new MockSpecialStoragePolicy;
    ResetQuotaManager(false /* is_incognito */);
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    quota_manager_ = NULL;
    scoped_task_environment_.RunUntilIdle();
  }

 protected:
  void ResetQuotaManager(bool is_incognito) {
    quota_manager_ = new QuotaManager(is_incognito, data_dir_.GetPath(),
                                      base::ThreadTaskRunnerHandle::Get().get(),
                                      mock_special_storage_policy_.get(),
                                      storage::GetQuotaSettingsFunc());
    SetQuotaSettings(kDefaultPoolSize, kDefaultPerHostQuota,
                     is_incognito ? INT64_C(0) : kMustRemainAvailableForSystem);

    // Don't (automatically) start the eviction for testing.
    quota_manager_->eviction_disabled_ = true;
    // Don't query the hard disk for remaining capacity.
    quota_manager_->get_volume_info_fn_= &GetVolumeInfoForTests;
    additional_callback_count_ = 0;
  }

  MockStorageClient* CreateClient(
      const MockOriginData* mock_data,
      size_t mock_data_size,
      QuotaClient::ID id) {
    return new MockStorageClient(quota_manager_->proxy(),
                                 mock_data, id, mock_data_size);
  }

  void RegisterClient(MockStorageClient* client) {
    quota_manager_->proxy()->RegisterClient(client);
  }

  void GetUsageInfo() {
    usage_info_.clear();
    quota_manager_->GetUsageInfo(
        base::Bind(&QuotaManagerTest::DidGetUsageInfo,
                   weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForWebApps(const GURL& origin,
                                  StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuotaForWebApps(
        origin, type, base::Bind(&QuotaManagerTest::DidGetUsageAndQuota,
                                 weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaWithBreakdown(const GURL& origin, StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    usage_breakdown_.clear();
    quota_manager_->GetUsageAndQuotaWithBreakdown(
        origin, type,
        base::Bind(&QuotaManagerTest::DidGetUsageAndQuotaWithBreakdown,
                   weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForStorageClient(const GURL& origin,
                                        StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuota(
        origin, type, base::Bind(&QuotaManagerTest::DidGetUsageAndQuota,
                                 weak_factory_.GetWeakPtr()));
  }

  void SetQuotaSettings(int64_t pool_size,
                        int64_t per_host_quota,
                        int64_t must_remain_available) {
    storage::QuotaSettings settings;
    settings.pool_size = pool_size;
    settings.per_host_quota = per_host_quota;
    settings.session_only_per_host_quota =
        (per_host_quota > 0) ? (per_host_quota - 1) : 0;
    settings.must_remain_available = must_remain_available;
    settings.refresh_interval = base::TimeDelta::Max();
    quota_manager_->SetQuotaSettings(settings);
  }

  void GetPersistentHostQuota(const std::string& host) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_->GetPersistentHostQuota(
        host,
        base::Bind(&QuotaManagerTest::DidGetHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void SetPersistentHostQuota(const std::string& host, int64_t new_quota) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_->SetPersistentHostQuota(
        host, new_quota,
        base::Bind(&QuotaManagerTest::DidGetHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void GetGlobalUsage(StorageType type) {
    usage_ = -1;
    unlimited_usage_ = -1;
    quota_manager_->GetGlobalUsage(
        type,
        base::Bind(&QuotaManagerTest::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
  }

  void GetHostUsage(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_->GetHostUsage(
        host, type,
        base::Bind(&QuotaManagerTest::DidGetHostUsage,
                   weak_factory_.GetWeakPtr()));
  }

  void GetHostUsageBreakdown(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_->GetHostUsageWithBreakdown(
        host, type,
        base::Bind(&QuotaManagerTest::DidGetHostUsageBreakdown,
                   weak_factory_.GetWeakPtr()));
  }

  void RunAdditionalUsageAndQuotaTask(const GURL& origin, StorageType type) {
    quota_manager_->GetUsageAndQuota(
        origin, type,
        base::Bind(&QuotaManagerTest::DidGetUsageAndQuotaAdditional,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteClientOriginData(QuotaClient* client,
                              const GURL& origin,
                              StorageType type) {
    DCHECK(client);
    quota_status_ = QuotaStatusCode::kUnknown;
    client->DeleteOriginData(
        origin, type,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void EvictOriginData(const GURL& origin,
                       StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->EvictOriginData(
        origin, type,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteOriginData(const GURL& origin,
                        StorageType type,
                        int quota_client_mask) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->DeleteOriginData(
        origin, type, quota_client_mask,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteHostData(const std::string& host,
                      StorageType type,
                      int quota_client_mask) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->DeleteHostData(
        host, type, quota_client_mask,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void GetStorageCapacity() {
    available_space_ = -1;
    total_space_ = -1;
    quota_manager_->GetStorageCapacity(base::Bind(
        &QuotaManagerTest::DidGetStorageCapacity, weak_factory_.GetWeakPtr()));
  }

  void GetEvictionRoundInfo() {
    quota_status_ = QuotaStatusCode::kUnknown;
    settings_ = storage::QuotaSettings();
    available_space_ = -1;
    total_space_ = -1;
    usage_ = -1;
    quota_manager_->GetEvictionRoundInfo(
        base::Bind(&QuotaManagerTest::DidGetEvictionRoundInfo,
                   weak_factory_.GetWeakPtr()));
  }

  void GetCachedOrigins(StorageType type, std::set<GURL>* origins) {
    ASSERT_TRUE(origins != NULL);
    origins->clear();
    quota_manager_->GetCachedOrigins(type, origins);
  }

  void NotifyStorageAccessed(QuotaClient* client,
                             const GURL& origin,
                             StorageType type) {
    DCHECK(client);
    quota_manager_->NotifyStorageAccessedInternal(
        client->id(), origin, type, IncrementMockTime());
  }

  void DeleteOriginFromDatabase(const GURL& origin, StorageType type) {
    quota_manager_->DeleteOriginFromDatabase(origin, type, false);
  }

  void GetEvictionOrigin(StorageType type) {
    eviction_origin_ = GURL();
    // The quota manager's default eviction policy is to use an LRU eviction
    // policy.
    quota_manager_->GetEvictionOrigin(
        type, std::set<GURL>(), 0,
        base::Bind(&QuotaManagerTest::DidGetEvictionOrigin,
                   weak_factory_.GetWeakPtr()));
  }

  void NotifyOriginInUse(const GURL& origin) {
    quota_manager_->NotifyOriginInUse(origin);
  }

  void NotifyOriginNoLongerInUse(const GURL& origin) {
    quota_manager_->NotifyOriginNoLongerInUse(origin);
  }

  void GetOriginsModifiedSince(StorageType type, base::Time modified_since) {
    modified_origins_.clear();
    modified_origins_type_ = StorageType::kUnknown;
    quota_manager_->GetOriginsModifiedSince(
        type, modified_since,
        base::Bind(&QuotaManagerTest::DidGetModifiedOrigins,
                   weak_factory_.GetWeakPtr()));
  }

  void DumpQuotaTable() {
    quota_entries_.clear();
    quota_manager_->DumpQuotaTable(
        base::Bind(&QuotaManagerTest::DidDumpQuotaTable,
                   weak_factory_.GetWeakPtr()));
  }

  void DumpOriginInfoTable() {
    origin_info_entries_.clear();
    quota_manager_->DumpOriginInfoTable(
        base::Bind(&QuotaManagerTest::DidDumpOriginInfoTable,
                   weak_factory_.GetWeakPtr()));
  }

  void DidGetUsageInfo(const UsageInfoEntries& entries) {
    usage_info_.insert(usage_info_.begin(), entries.begin(), entries.end());
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
      base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
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
      base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
    usage_ = usage;
    usage_breakdown_ = std::move(usage_breakdown);
  }

  void DidGetEvictionRoundInfo(QuotaStatusCode status,
                               const storage::QuotaSettings& settings,
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

  void DidGetEvictionOrigin(const GURL& origin) {
    eviction_origin_ = origin;
  }

  void DidGetModifiedOrigins(const std::set<GURL>& origins, StorageType type) {
    modified_origins_ = origins;
    modified_origins_type_ = type;
  }

  void DidDumpQuotaTable(const QuotaTableEntries& entries) {
    quota_entries_ = entries;
  }

  void DidDumpOriginInfoTable(const OriginInfoTableEntries& entries) {
    origin_info_entries_ = entries;
  }

  void GetUsage_WithModifyTestBody(const StorageType type);

  void set_additional_callback_count(int c) { additional_callback_count_ = c; }
  int additional_callback_count() const { return additional_callback_count_; }
  void DidGetUsageAndQuotaAdditional(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    ++additional_callback_count_;
  }

  QuotaManager* quota_manager() const { return quota_manager_.get(); }
  void set_quota_manager(QuotaManager* quota_manager) {
    quota_manager_ = quota_manager;
  }

  MockSpecialStoragePolicy* mock_special_storage_policy() const {
    return mock_special_storage_policy_.get();
  }

  QuotaStatusCode status() const { return quota_status_; }
  const UsageInfoEntries& usage_info() const { return usage_info_; }
  int64_t usage() const { return usage_; }
  const base::flat_map<QuotaClient::ID, int64_t>& usage_breakdown() const {
    return usage_breakdown_;
  }
  int64_t limited_usage() const { return limited_usage_; }
  int64_t unlimited_usage() const { return unlimited_usage_; }
  int64_t quota() const { return quota_; }
  int64_t total_space() const { return total_space_; }
  int64_t available_space() const { return available_space_; }
  const GURL& eviction_origin() const { return eviction_origin_; }
  const std::set<GURL>& modified_origins() const { return modified_origins_; }
  StorageType modified_origins_type() const { return modified_origins_type_; }
  const QuotaTableEntries& quota_entries() const { return quota_entries_; }
  const OriginInfoTableEntries& origin_info_entries() const {
    return origin_info_entries_;
  }
  const storage::QuotaSettings& settings() const { return settings_; }
  base::FilePath profile_path() const { return data_dir_.GetPath(); }
  int status_callback_count() const { return status_callback_count_; }
  void reset_status_callback_count() { status_callback_count_ = 0; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
  }

  base::ScopedTempDir data_dir_;

  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  UsageInfoEntries usage_info_;
  int64_t usage_;
  base::flat_map<QuotaClient::ID, int64_t> usage_breakdown_;
  int64_t limited_usage_;
  int64_t unlimited_usage_;
  int64_t quota_;
  int64_t total_space_;
  int64_t available_space_;
  GURL eviction_origin_;
  std::set<GURL> modified_origins_;
  StorageType modified_origins_type_;
  QuotaTableEntries quota_entries_;
  OriginInfoTableEntries origin_info_entries_;
  storage::QuotaSettings settings_;
  int status_callback_count_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerTest);
};

TEST_F(QuotaManagerTest, GetUsageInfo) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",       kTemp,  10 },
    { "http://foo.com:8080/",  kTemp,  15 },
    { "http://bar.com/",       kTemp,  20 },
    { "http://bar.com/",       kPerm,  50 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",      kTemp,  30 },
    { "https://foo.com:8081/", kTemp,  35 },
    { "http://bar.com/",       kPerm,  40 },
    { "http://example.com/",   kPerm,  40 },
  };
  RegisterClient(CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem));
  RegisterClient(CreateClient(kData2, arraysize(kData2),
      QuotaClient::kDatabase));

  GetUsageInfo();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(4U, usage_info().size());
  for (size_t i = 0; i < usage_info().size(); ++i) {
    const UsageInfo& info = usage_info()[i];
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

TEST_F(QuotaManagerTest, GetUsageAndQuota_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(0, quota());
  int64_t quota_returned_for_foo = quota();

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(quota_returned_for_foo, quota());
}

TEST_F(QuotaManagerTest, GetUsage_NoClient) {
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetUsage_EmptyClient) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp,  10 },
    { "http://foo.com:8080/",   kTemp,  20 },
    { "http://bar.com/",        kTemp,   5 },
    { "https://bar.com/",       kTemp,   7 },
    { "http://baz.com/",        kTemp,  30 },
    { "http://foo.com/",        kPerm,  40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  // This time explicitly sets a temporary global quota.
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  // The host's quota should be its full portion of the global quota
  // since there's plenty of diskspace.
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerTest, GetUsage_MultipleClients) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",              kTemp, 1 },
    { "http://bar.com/",              kTemp, 2 },
    { "http://bar.com/",              kPerm, 4 },
    { "http://unlimited/",            kPerm, 8 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",             kTemp, 128 },
    { "http://example.com/",          kPerm, 256 },
    { "http://unlimited/",            kTemp, 512 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  RegisterClient(CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem));
  RegisterClient(CreateClient(kData2, arraysize(kData2),
      QuotaClient::kDatabase));

  const int64_t kPoolSize = GetAvailableDiskSpaceForTest();
  const int64_t kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 2 + 128 + 512, usage());
  EXPECT_EQ(512, unlimited_usage());

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4 + 8 + 256, usage());
  EXPECT_EQ(8, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_Simple) {
  base::flat_map<QuotaClient::ID, int64_t> usage_breakdown_expected;
  static const MockOriginData kData1[] = {
      {"http://foo.com/", kTemp, 1}, {"http://foo.com/", kPerm, 80},
  };
  static const MockOriginData kData2[] = {
      {"http://foo.com/", kTemp, 4},
  };
  static const MockOriginData kData3[] = {
      {"http://foo.com/", kTemp, 8},
  };
  MockStorageClient* client1 =
      CreateClient(kData1, arraysize(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, arraysize(kData2), QuotaClient::kDatabase);
  MockStorageClient* client3 =
      CreateClient(kData3, arraysize(kData3), QuotaClient::kAppcache);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 80;
  usage_breakdown_expected[QuotaClient::kDatabase] = 0;
  usage_breakdown_expected[QuotaClient::kAppcache] = 0;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 4 + 8, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 1;
  usage_breakdown_expected[QuotaClient::kDatabase] = 4;
  usage_breakdown_expected[QuotaClient::kAppcache] = 8;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://bar.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 0;
  usage_breakdown_expected[QuotaClient::kDatabase] = 0;
  usage_breakdown_expected[QuotaClient::kAppcache] = 0;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_NoClient) {
  base::flat_map<QuotaClient::ID, int64_t> usage_breakdown_expected;

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetHostUsageBreakdown("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetHostUsageBreakdown("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_MultiOrigins) {
  base::flat_map<QuotaClient::ID, int64_t> usage_breakdown_expected;
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 10}, {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 5},  {"https://bar.com/", kTemp, 7},
      {"http://baz.com/", kTemp, 30}, {"http://foo.com/", kPerm, 40},
  };
  RegisterClient(
      CreateClient(kData, arraysize(kData), QuotaClient::kFileSystem));

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 10 + 20;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://bar.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 5 + 7;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_MultipleClients) {
  base::flat_map<QuotaClient::ID, int64_t> usage_breakdown_expected;
  static const MockOriginData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://bar.com/", kTemp, 2},
      {"http://bar.com/", kPerm, 4},
      {"http://unlimited/", kPerm, 8},
  };
  static const MockOriginData kData2[] = {
      {"https://foo.com/", kTemp, 128},
      {"http://example.com/", kPerm, 256},
      {"http://unlimited/", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  RegisterClient(
      CreateClient(kData1, arraysize(kData1), QuotaClient::kFileSystem));
  RegisterClient(
      CreateClient(kData2, arraysize(kData2), QuotaClient::kDatabase));

  GetUsageAndQuotaWithBreakdown(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 1;
  usage_breakdown_expected[QuotaClient::kDatabase] = 128;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://bar.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 4;
  usage_breakdown_expected[QuotaClient::kDatabase] = 0;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 0;
  usage_breakdown_expected[QuotaClient::kDatabase] = 512;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());

  GetUsageAndQuotaWithBreakdown(GURL("http://unlimited/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  usage_breakdown_expected[QuotaClient::kFileSystem] = 8;
  usage_breakdown_expected[QuotaClient::kDatabase] = 0;
  EXPECT_EQ(usage_breakdown_expected, usage_breakdown());
}

void QuotaManagerTest::GetUsage_WithModifyTestBody(const StorageType type) {
  const MockOriginData data[] = {
    { "http://foo.com/",   type,  10 },
    { "http://foo.com:1/", type,  20 },
  };
  MockStorageClient* client = CreateClient(data, arraysize(data),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), type);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  client->ModifyOriginAndNotify(GURL("http://foo.com/"), type, 30);
  client->ModifyOriginAndNotify(GURL("http://foo.com:1/"), type, -5);
  client->AddOriginAndNotify(GURL("https://foo.com/"), type, 1);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), type);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 30 - 5 + 1, usage());
  int foo_usage = usage();

  client->AddOriginAndNotify(GURL("http://bar.com/"), type, 40);
  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), type);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(40, usage());

  GetGlobalUsage(type);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(foo_usage + 40, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsage_WithModify) {
  GetUsage_WithModifyTestBody(kTemp);
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"),
                                 kTemp);

  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://bar.com/"), kTemp, kAllClients);

  // Nuke before waiting for callbacks.
  set_quota_manager(NULL);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const MockOriginData kData[] = {
    { "http://usage1/",    kTemp,   1 },
    { "http://usage10/",   kTemp,  10 },
    { "http://usage200/",  kTemp, 200 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  // Provided diskspace is not tight, global usage does not affect the
  // quota calculations for an individual origin, so despite global usage
  // in excess of our poolsize, we still get the nominal quota value.
  GetStorageCapacity();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_LE(kMustRemainAvailableForSystem, available_space());

  GetUsageAndQuotaForWebApps(GURL("http://usage1/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage200/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(200, usage());
  EXPECT_EQ(kPerHostQuota, quota());  // should be clamped to the nominal quota
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const MockOriginData kData[] = {
    { "http://usage10/",   kTemp,    10 },
    { "http://usage50/",   kTemp,    50 },
    { "http://unlimited/", kTemp,  4000 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  // Test when not overbugdet.
  const int kPerHostQuotaFor1000 = 200;
  SetQuotaSettings(1000, kPerHostQuotaFor1000, kMustRemainAvailableForSystem);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(4000, unlimited_usage());

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Test when overbugdet.
  const int kPerHostQuotaFor100 = 20;
  SetQuotaSettings(100, kPerHostQuotaFor100, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());
}

TEST_F(QuotaManagerTest, OriginInUse) {
  const GURL kFooOrigin("http://foo.com/");
  const GURL kBarOrigin("http://bar.com/");

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 2
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginInUse(kBarOrigin);
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kBarOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));

  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
}

TEST_F(QuotaManagerTest, GetAndSetPerststentHostQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  GetPersistentHostQuota("foo.com");
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(100, quota());

  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", 200);
  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", QuotaManager::kPerHostPersistentQuotaLimit);
  GetPersistentHostQuota("foo.com");
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());

  // Persistent quota should be capped at the per-host quota limit.
  SetPersistentHostQuota("foo.com",
                         QuotaManager::kPerHostPersistentQuotaLimit + 100);
  GetPersistentHostQuota("foo.com");
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());
}

TEST_F(QuotaManagerTest, GetAndSetPersistentUsageAndQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(100, quota());

  // The actual space avaialble is given to 'unlimited' origins as their quota.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(kAvailableSpaceForApp, quota());

  // GetUsageAndQuotaForStorageClient should just return 0 usage and
  // kNoLimit quota.
  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());
}

TEST_F(QuotaManagerTest, GetSyncableQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  // Pre-condition check: available disk space (for testing) is less than
  // the default quota for syncable storage.
  EXPECT_LE(kAvailableSpaceForApp,
            QuotaManager::kSyncableStorageDefaultHostQuota);

  // For unlimited origins the quota manager should return
  // kAvailableSpaceForApp as syncable quota (because of the pre-condition).
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kSync);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(kAvailableSpaceForApp, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm, 10 },
    { "http://foo.com:8080/",   kPerm, 20 },
    { "https://foo.com/",       kPerm, 13 },
    { "https://foo.com:8081/",  kPerm, 19 },
    { "http://bar.com/",        kPerm,  5 },
    { "https://bar.com/",       kPerm,  7 },
    { "http://baz.com/",        kPerm, 30 },
    { "http://foo.com/",        kTemp, 40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 13 + 19, usage());
  EXPECT_EQ(100, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsage_WithModify) {
  GetUsage_WithModifyTestBody(kPerm);
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(100, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kPerm);

  // Nuke before waiting for callbacks.
  set_quota_manager(NULL);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetUsage_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20);

  GetHostUsage("buz.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);
}

TEST_F(QuotaManagerTest, GetUsage_WithModification) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };

  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(
      GURL("http://foo.com/"), kPerm, 80000000);

  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(
      GURL("http://foo.com/"), kTemp, 1);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("buz.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);

  client->ModifyOriginAndNotify(
      GURL("http://buz.com/"), kTemp, 900000000);

  GetHostUsage("buz.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000 + 900000000);
}

TEST_F(QuotaManagerTest, GetUsage_WithDeleteOrigin) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteClientOriginData(client, GURL("http://foo.com/"),
                         kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetStorageCapacity) {
  GetStorageCapacity();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_LE(0, total_space());
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, EvictOriginData) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (size_t i = 0; i < arraysize(kData1); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData1[i].origin), kData1[i].type);
  for (size_t i = 0; i < arraysize(kData2); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData2[i].origin), kData2[i].type);
  scoped_task_environment_.RunUntilIdle();

  EvictOriginData(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();

  DumpOriginInfoTable();
  scoped_task_environment_.RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp)
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
  }

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, EvictOriginDataHistogram) {
  const GURL kOrigin = GURL("http://foo.com/");
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 1},
  };

  base::HistogramTester histograms;
  MockStorageClient* client =
      CreateClient(kData, arraysize(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();

  EvictOriginData(kOrigin, kTemp);
  scoped_task_environment_.RunUntilIdle();

  // Ensure used count and time since access are recorded.
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 1);
  histograms.ExpectBucketCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 0, 1);
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginDaysSinceAccessHistogram, 1);

  // First eviction has no 'last' time to compare to.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 0);

  client->AddOriginAndNotify(kOrigin, kTemp, 100);

  // Change the used count of the origin.
  quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown, GURL(kOrigin),
                                         kTemp);
  scoped_task_environment_.RunUntilIdle();

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();

  EvictOriginData(kOrigin, kTemp);
  scoped_task_environment_.RunUntilIdle();

  // The new used count should be logged.
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 2);
  histograms.ExpectBucketCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 1, 1);
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginDaysSinceAccessHistogram, 2);

  // Second eviction should log a 'time between repeated eviction' sample.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 1);

  client->AddOriginAndNotify(kOrigin, kTemp, 100);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();

  DeleteOriginFromDatabase(kOrigin, kTemp);

  // Deletion from non-eviction source should not log a histogram sample.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 1);
}

TEST_F(QuotaManagerTest, EvictOriginDataWithDeletionError) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://bar.com/",   kTemp,    4000 },
  };
  static const int kNumberOfTemporaryOrigins = 3;
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (size_t i = 0; i < arraysize(kData); ++i)
    NotifyStorageAccessed(client, GURL(kData[i].origin), kData[i].type);
  scoped_task_environment_.RunUntilIdle();

  client->AddOriginToErrorSet(GURL("http://foo.com/"), kTemp);

  for (int i = 0;
       i < QuotaManager::kThresholdOfErrorsToBeBlacklisted + 1;
       ++i) {
    EvictOriginData(GURL("http://foo.com/"), kTemp);
    scoped_task_environment_.RunUntilIdle();
    EXPECT_EQ(QuotaStatusCode::kErrorInvalidModification, status());
  }

  DumpOriginInfoTable();
  scoped_task_environment_.RunUntilIdle();

  bool found_origin_in_database = false;
  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp && itr->origin == "http://foo.com/") {
      found_origin_in_database = true;
      break;
    }
  }
  // The origin "http://foo.com/" should be in the database.
  EXPECT_TRUE(found_origin_in_database);

  for (size_t i = 0; i < kNumberOfTemporaryOrigins - 1; ++i) {
    GetEvictionOrigin(kTemp);
    scoped_task_environment_.RunUntilIdle();
    EXPECT_FALSE(eviction_origin().is_empty());
    // The origin "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"), eviction_origin().spec());
    DeleteOriginFromDatabase(eviction_origin(), kTemp);
    scoped_task_environment_.RunUntilIdle();
  }

  // Now the LRU list must be empty.
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(eviction_origin().is_empty());

  // Deleting origins from the database should not affect the results of the
  // following checks.
  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetEvictionRoundInfo) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://unlimited/", kTemp,    4000 },
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetEvictionRoundInfo();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(21, usage());
  EXPECT_EQ(kPoolSize, settings().pool_size);
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, DeleteHostDataSimple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteHostData(std::string(), kTemp, kAllClients);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());

  DeleteHostData("foo.com", kTemp, kAllClients);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, DeleteHostDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  reset_status_callback_count();
  DeleteHostData("foo.com", kTemp, kAllClients);
  DeleteHostData("bar.com", kTemp, kAllClients);
  DeleteHostData("foo.com", kTemp, kAllClients);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  scoped_task_environment_.RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp) {
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://foo.com:1/"), itr->origin.spec());
      EXPECT_NE(std::string("https://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://bar.com/"), itr->origin.spec());
    }
  }

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9),
            usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 20 + 50000 + 6000 + 80), usage());

  GetHostUsage("bar.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

// Single-run DeleteOriginData cases must be well covered by
// EvictOriginData tests.
TEST_F(QuotaManagerTest, DeleteOriginDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  for (size_t i = 0; i < arraysize(kData1); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData1[i].origin), kData1[i].type);
  for (size_t i = 0; i < arraysize(kData2); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData2[i].origin), kData2[i].type);
  scoped_task_environment_.RunUntilIdle();

  reset_status_callback_count();
  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://bar.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  scoped_task_environment_.RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp) {
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://bar.com/"), itr->origin.spec());
    }
  }

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 4000 + 50000 + 9), usage());

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 50000), usage());

  GetHostUsage("bar.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerTest, GetCachedOrigins) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,       1 },
    { "http://a.com:1/", kTemp,      20 },
    { "http://b.com/",   kPerm,     300 },
    { "http://c.com/",   kTemp,    4000 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  // TODO(kinuko): Be careful when we add cache pruner.

  std::set<GURL> origins;
  GetCachedOrigins(kTemp, &origins);
  EXPECT_TRUE(origins.empty());

  GetHostUsage("a.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(2U, origins.size());

  GetHostUsage("b.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(2U, origins.size());

  GetHostUsage("c.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  GetCachedOrigins(kPerm, &origins);
  EXPECT_TRUE(origins.empty());

  GetGlobalUsage(kTemp);
  scoped_task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  for (size_t i = 0; i < arraysize(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_TRUE(origins.find(GURL(kData[i].origin)) != origins.end());
  }
}

TEST_F(QuotaManagerTest, NotifyAndLRUOrigin) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(eviction_origin().is_empty());

  NotifyStorageAccessed(client, GURL("http://a.com/"), kTemp);
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin().spec());

  NotifyStorageAccessed(client, GURL("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, GURL("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin().spec());

  DeleteOriginFromDatabase(eviction_origin(), kTemp);
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("https://a.com/", eviction_origin().spec());

  DeleteOriginFromDatabase(eviction_origin(), kTemp);
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("http://c.com/", eviction_origin().spec());
}

TEST_F(QuotaManagerTest, GetLRUOriginWithOriginInUse) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(eviction_origin().is_empty());

  NotifyStorageAccessed(client, GURL("http://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, GURL("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);

  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin().spec());

  // Notify origin http://a.com is in use.
  NotifyOriginInUse(GURL("http://a.com/"));
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("https://a.com/", eviction_origin().spec());

  // Notify origin https://a.com is in use while GetEvictionOrigin is running.
  GetEvictionOrigin(kTemp);
  NotifyOriginInUse(GURL("https://a.com/"));
  scoped_task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_TRUE(eviction_origin().is_empty());

  // Notify access for http://c.com while GetEvictionOrigin is running.
  GetEvictionOrigin(kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_TRUE(eviction_origin().is_empty());

  NotifyOriginNoLongerInUse(GURL("http://a.com/"));
  NotifyOriginNoLongerInUse(GURL("https://a.com/"));
  GetEvictionOrigin(kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin().spec());
}

TEST_F(QuotaManagerTest, GetOriginsModifiedSince) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetOriginsModifiedSince(kTemp, base::Time());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  base::Time time1 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(GURL("http://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://a.com:1/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://b.com/"), kPerm, 10);
  base::Time time2 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(GURL("https://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://c.com/"), kTemp, 10);
  base::Time time3 = client->IncrementMockTime();

  GetOriginsModifiedSince(kTemp, time1);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(4U, modified_origins().size());
  EXPECT_EQ(modified_origins_type(), kTemp);
  for (size_t i = 0; i < arraysize(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_EQ(1U, modified_origins().count(GURL(kData[i].origin)));
  }

  GetOriginsModifiedSince(kTemp, time2);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(2U, modified_origins().size());

  GetOriginsModifiedSince(kTemp, time3);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  client->ModifyOriginAndNotify(GURL("http://a.com/"), kTemp, 10);

  GetOriginsModifiedSince(kTemp, time3);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, modified_origins().size());
  EXPECT_EQ(1U, modified_origins().count(GURL("http://a.com/")));
  EXPECT_EQ(modified_origins_type(), kTemp);
}

TEST_F(QuotaManagerTest, DumpQuotaTable) {
  SetPersistentHostQuota("example1.com", 1);
  SetPersistentHostQuota("example2.com", 20);
  SetPersistentHostQuota("example3.com", 300);
  scoped_task_environment_.RunUntilIdle();

  DumpQuotaTable();
  scoped_task_environment_.RunUntilIdle();

  const QuotaTableEntry kEntries[] = {
    QuotaTableEntry("example1.com", kPerm, 1),
    QuotaTableEntry("example2.com", kPerm, 20),
    QuotaTableEntry("example3.com", kPerm, 300),
  };
  std::set<QuotaTableEntry> entries(kEntries, kEntries + arraysize(kEntries));

  typedef QuotaTableEntries::const_iterator iterator;
  for (iterator itr(quota_entries().begin()), end(quota_entries().end());
       itr != end; ++itr) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << itr->host << ", "
                 << "quota = " << itr->quota);
    EXPECT_EQ(1u, entries.erase(*itr));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, DumpOriginInfoTable) {
  using std::make_pair;

  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kTemp);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kPerm);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kPerm);
  scoped_task_environment_.RunUntilIdle();

  DumpOriginInfoTable();
  scoped_task_environment_.RunUntilIdle();

  typedef std::pair<GURL, StorageType> TypedOrigin;
  typedef std::pair<TypedOrigin, int> Entry;
  const Entry kEntries[] = {
    make_pair(make_pair(GURL("http://example.com/"), kTemp), 1),
    make_pair(make_pair(GURL("http://example.com/"), kPerm), 2),
  };
  std::set<Entry> entries(kEntries, kEntries + arraysize(kEntries));

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << itr->origin << ", "
                 << "type = " << static_cast<int>(itr->type) << ", "
                 << "used_count = " << itr->used_count);
    EXPECT_EQ(1u, entries.erase(
        make_pair(make_pair(itr->origin, itr->type),
                  itr->used_count)));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, QuotaForEmptyHost) {
  GetPersistentHostQuota(std::string());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota(std::string(), 10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorNotSupported, status());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, arraysize(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, arraysize(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kFileSystem);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kAppcache);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kIndexedDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, arraysize(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, arraysize(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp, QuotaClient::kFileSystem);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kAppcache);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kIndexedDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, arraysize(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, arraysize(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kFileSystem | QuotaClient::kDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kAppcache | QuotaClient::kIndexedDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, arraysize(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, arraysize(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, arraysize(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, arraysize(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kFileSystem | QuotaClient::kAppcache);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kDatabase | QuotaClient::kIndexedDatabase);
  scoped_task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManager(true);

  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(CreateClient(kData, arraysize(kData),
      QuotaClient::kFileSystem));

  // Query global usage to warmup the usage tracker caching.
  GetGlobalUsage(kTemp);
  GetGlobalUsage(kPerm);
  scoped_task_environment_.RunUntilIdle();

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  const int kPoolSize = 1000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, INT64_C(0));

  GetStorageCapacity();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(kPoolSize, total_space());
  EXPECT_EQ(kPoolSize - 80 - 10, available_space());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(kPerHostQuota, quota());

  mock_special_storage_policy()->AddUnlimited(GURL("http://foo.com/"));
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(available_space() + usage(), quota());
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_SessionOnly) {
  const GURL kEpheremalOrigin("http://ephemeral/");
  mock_special_storage_policy()->AddSessionOnly(kEpheremalOrigin);

  GetUsageAndQuotaForWebApps(kEpheremalOrigin, kTemp);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(quota_manager()->settings().session_only_per_host_quota, quota());

  GetUsageAndQuotaForWebApps(kEpheremalOrigin, kPerm);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());
}

}  // namespace content
