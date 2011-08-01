// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <set>

#include "base/file_util.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/mock_storage_client.h"

namespace quota {

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest()
    : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      deletion_callback_count_(0) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = new MockSpecialStoragePolicy;
    manager_ = new MockQuotaManager(
        false /* is_incognito */,
        data_dir_.path(),
        base::MessageLoopProxy::CreateForCurrentThread(),
        base::MessageLoopProxy::CreateForCurrentThread(),
        policy_);
  }

  void TearDown() {
    // Make sure the quota manager cleans up correctly.
    manager_ = NULL;
    MessageLoop::current()->RunAllPending();
  }

  void GetModifiedOrigins(StorageType type, base::Time since) {
    manager_->GetOriginsModifiedSince(type, since,
        callback_factory_.NewCallback(
            &MockQuotaManagerTest::GotModifiedOrigins));
  }

  void GotModifiedOrigins(const std::set<GURL>& origins) {
    origins_ = origins;
  }

  void DeleteOriginData(const GURL& origin, StorageType type) {
    manager_->DeleteOriginData(origin, type,
        callback_factory_.NewCallback(
            &MockQuotaManagerTest::DeletedOriginData));
  }

  void DeletedOriginData(QuotaStatusCode status) {
    ++deletion_callback_count_;
    EXPECT_EQ(quota::kQuotaStatusOk, status);
  }

  int GetDeletionCallbackCount() {
    return deletion_callback_count_;
  }

  MockQuotaManager* GetManager() {
    return manager_.get();
  }

  const std::set<GURL> GetOrigins() {
    return origins_;
  }

 private:
  ScopedTempDir data_dir_;
  base::ScopedCallbackFactory<MockQuotaManagerTest> callback_factory_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<GURL> origins_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerTest);
};

TEST_F(MockQuotaManagerTest, BasicOriginManipulation) {
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin1, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin1, kStorageTypePersistent));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypePersistent));

  GetManager()->AddOrigin(kOrigin1, kStorageTypeTemporary, base::Time::Now());
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin1, kStorageTypePersistent));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypePersistent));

  GetManager()->AddOrigin(kOrigin1, kStorageTypePersistent, base::Time::Now());
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypeTemporary));
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypePersistent));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypePersistent));

  GetManager()->AddOrigin(kOrigin2, kStorageTypeTemporary, base::Time::Now());
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypeTemporary));
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin2, kStorageTypeTemporary));
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypePersistent));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypePersistent));
}

TEST_F(MockQuotaManagerTest, OriginDeletion) {
  GetManager()->AddOrigin(kOrigin1, kStorageTypeTemporary, base::Time::Now());
  GetManager()->AddOrigin(kOrigin2, kStorageTypeTemporary, base::Time::Now());

  DeleteOriginData(kOrigin2, kStorageTypeTemporary);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1, GetDeletionCallbackCount());
  EXPECT_TRUE(GetManager()->OriginHasData(kOrigin1, kStorageTypeTemporary));
  EXPECT_FALSE(GetManager()->OriginHasData(kOrigin2, kStorageTypeTemporary));
}

TEST_F(MockQuotaManagerTest, ModifiedOrigins) {
  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::TimeDelta::FromMilliseconds(3600000);
  base::TimeDelta a_minute = base::TimeDelta::FromMilliseconds(60000);

  GetModifiedOrigins(kStorageTypeTemporary, then);
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(GetOrigins().empty());

  GetManager()->AddOrigin(kOrigin1, kStorageTypeTemporary, now - an_hour);

  GetModifiedOrigins(kStorageTypeTemporary, then);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1UL, GetOrigins().size());
  EXPECT_EQ(1UL, GetOrigins().count(kOrigin1));
  EXPECT_EQ(0UL, GetOrigins().count(kOrigin2));

  GetManager()->AddOrigin(kOrigin2, kStorageTypeTemporary, now);

  GetModifiedOrigins(kStorageTypeTemporary, then);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(2UL, GetOrigins().size());
  EXPECT_EQ(1UL, GetOrigins().count(kOrigin1));
  EXPECT_EQ(1UL, GetOrigins().count(kOrigin2));

  GetModifiedOrigins(kStorageTypeTemporary, now - a_minute);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1UL, GetOrigins().size());
  EXPECT_EQ(0UL, GetOrigins().count(kOrigin1));
  EXPECT_EQ(1UL, GetOrigins().count(kOrigin2));
}
}  // Namespace quota
