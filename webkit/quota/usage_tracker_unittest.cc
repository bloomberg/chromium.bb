// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/usage_tracker.h"

namespace quota {

namespace {

void DidGetGlobalUsage(bool* done,
                       StorageType* type_out,
                       int64* usage_out,
                       int64* unlimited_usage_out,
                       StorageType type,
                       int64 usage,
                       int64 unlimited_usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *type_out = type;
  *usage_out = usage;
  *unlimited_usage_out = unlimited_usage;
}

void DidGetUsage(bool* done,
                 int64* usage_out,
                 int64 usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
}

}  // namespace

class MockQuotaClient : public QuotaClient {
 public:
  MockQuotaClient() {}
  virtual ~MockQuotaClient() {}

  virtual ID id() const OVERRIDE {
    return kFileSystem;
  }

  virtual void OnQuotaManagerDestroyed() OVERRIDE {}

  virtual void GetOriginUsage(const GURL& origin,
                              StorageType type,
                              const GetUsageCallback& callback) OVERRIDE {
    EXPECT_EQ(kStorageTypeTemporary, type);
    int64 usage = GetUsage(origin);
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, usage));
  }

  virtual void GetOriginsForType(StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE {
    EXPECT_EQ(kStorageTypeTemporary, type);
    std::set<GURL> origins;
    for (UsageMap::const_iterator itr = usage_map_.begin();
         itr != usage_map_.end(); ++itr) {
      origins.insert(itr->first);
    }
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, origins, type));
  }

  virtual void GetOriginsForHost(StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE {
    EXPECT_EQ(kStorageTypeTemporary, type);
    std::set<GURL> origins;
    for (UsageMap::const_iterator itr = usage_map_.begin();
         itr != usage_map_.end(); ++itr) {
      if (net::GetHostOrSpecFromURL(itr->first) == host)
        origins.insert(itr->first);
    }
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, origins, type));
  }

  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                const DeletionCallback& callback) OVERRIDE {
    EXPECT_EQ(kStorageTypeTemporary, type);
    usage_map_.erase(origin);
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, kQuotaStatusOk));
  }

  int64 GetUsage(const GURL& origin) {
    UsageMap::const_iterator found = usage_map_.find(origin);
    if (found == usage_map_.end())
      return 0;
    return found->second;
  }

  void SetUsage(const GURL& origin, int64 usage) {
    usage_map_[origin] = usage;
  }

  int64 UpdateUsage(const GURL& origin, int64 delta) {
    return usage_map_[origin] += delta;
  }

 private:
  typedef std::map<GURL, int64> UsageMap;

  UsageMap usage_map_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaClient);
};

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(new MockSpecialStoragePolicy()),
        usage_tracker_(GetUsageTrackerList(), kStorageTypeTemporary,
                       storage_policy_.get()) {
  }

  virtual ~UsageTrackerTest() {}

  UsageTracker* usage_tracker() {
    return &usage_tracker_;
  }

  void UpdateUsage(const GURL& origin, int64 delta) {
    quota_client_.UpdateUsage(origin, delta);
    usage_tracker_.UpdateUsageCache(quota_client_.id(), origin, delta);
    message_loop_.RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const GURL& origin, int64 delta) {
    quota_client_.UpdateUsage(origin, delta);
  }

  void GetGlobalUsage(int64* usage, int64* unlimited_usage) {
    bool done = false;
    StorageType type = kStorageTypeUnknown;
    usage_tracker_.GetGlobalUsage(base::Bind(
        &DidGetGlobalUsage,
        &done, &type, usage, unlimited_usage));
    message_loop_.RunUntilIdle();

    EXPECT_TRUE(done);
    EXPECT_EQ(kStorageTypeTemporary, type);
  }

  void GetHostUsage(const std::string& host, int64* usage) {
    bool done = false;
    usage_tracker_.GetHostUsage(host, base::Bind(&DidGetUsage, &done, usage));
    message_loop_.RunUntilIdle();

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

  MessageLoop message_loop_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  MockQuotaClient quota_client_;
  UsageTracker usage_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTest);
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64 usage = 0;
  int64 unlimited_usage = 0;
  int64 host_usage = 0;
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

  GrantUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  EXPECT_EQ(100, host_usage);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64 usage = 0;
  int64 unlimited_usage = 0;
  int64 host_usage = 0;

  const GURL origin("http://example.com");
  const std::string host(net::GetHostOrSpecFromURL(origin));

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);

  UpdateUsageWithoutNotification(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(100, host_usage);

  GrantUnlimitedStoragePolicy(origin);
  UpdateUsageWithoutNotification(origin, 100);
  SetUsageCacheEnabled(origin, false);
  UpdateUsageWithoutNotification(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  EXPECT_EQ(400, host_usage);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  EXPECT_EQ(400, host_usage);

  SetUsageCacheEnabled(origin, true);
  UpdateUsage(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  GetHostUsage(host, &host_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  EXPECT_EQ(500, host_usage);
}

}  // namespace quota
