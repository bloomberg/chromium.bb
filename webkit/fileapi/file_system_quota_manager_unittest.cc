// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_manager.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/special_storage_policy.h"

using namespace fileapi;

namespace {

static const char* const kTestOrigins[] = {
  "https://a.com/",
  "http://b.com/",
  "http://c.com:1/",
  "file:///",
};

class TestSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  virtual bool IsStorageProtected(const GURL& origin) {
    return false;
  }

  virtual bool IsStorageUnlimited(const GURL& origin) {
    return origin == GURL(kTestOrigins[1]);
  }
};

}  // anonymous namespace

TEST(FileSystemQuotaManagerTest, CheckNotAllowed) {
  FileSystemQuotaManager quota(false, false, NULL);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaNotAllowed #"
                 << i << " " << kTestOrigins[i]);
    // Should fail no matter how much size is requested.
    GURL origin(kTestOrigins[i]);
    EXPECT_FALSE(quota.CheckOriginQuota(origin, -1));
    EXPECT_FALSE(quota.CheckOriginQuota(origin, 0));
    EXPECT_FALSE(quota.CheckOriginQuota(origin, 100));
  }
}

TEST(FileSystemQuotaManagerTest, CheckUnlimitedFlag) {
  FileSystemQuotaManager quota(false, true, NULL);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaNotAllowed #"
                 << i << " " << kTestOrigins[i]);
    // Should succeed no matter how much size is requested.
    GURL origin(kTestOrigins[i]);
    EXPECT_TRUE(quota.CheckOriginQuota(origin, -1));
    EXPECT_TRUE(quota.CheckOriginQuota(origin, 0));
    EXPECT_TRUE(quota.CheckOriginQuota(origin, 100));
  }
}

TEST(FileSystemQuotaManagerTest, CheckAllowFileFlag) {
  FileSystemQuotaManager quota(true, false, NULL);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaNotAllowed #"
                 << i << " " << kTestOrigins[i]);
    // Should succeed only for file:// urls
    GURL origin(kTestOrigins[i]);
    if (origin.SchemeIsFile()) {
      EXPECT_TRUE(quota.CheckOriginQuota(origin, -1));
      EXPECT_TRUE(quota.CheckOriginQuota(origin, 0));
      EXPECT_TRUE(quota.CheckOriginQuota(origin, 100));
    } else {
      EXPECT_FALSE(quota.CheckOriginQuota(origin, -1));
      EXPECT_FALSE(quota.CheckOriginQuota(origin, 0));
      EXPECT_FALSE(quota.CheckOriginQuota(origin, 100));
    }
  }
}

TEST(FileSystemQuotaManagerTest, CheckSpecialPolicy) {
  scoped_refptr<TestSpecialStoragePolicy> policy(new TestSpecialStoragePolicy);

  FileSystemQuotaManager quota(false, false, policy);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaNotAllowed #"
                 << i << " " << kTestOrigins[i]);
    // Should succeed only for unlimited origins according to the policy.
    GURL origin(kTestOrigins[i]);
    if (policy->IsStorageUnlimited(origin)) {
      EXPECT_TRUE(quota.CheckOriginQuota(origin, -1));
      EXPECT_TRUE(quota.CheckOriginQuota(origin, 0));
      EXPECT_TRUE(quota.CheckOriginQuota(origin, 100));
    } else {
      EXPECT_FALSE(quota.CheckOriginQuota(origin, -1));
      EXPECT_FALSE(quota.CheckOriginQuota(origin, 0));
      EXPECT_FALSE(quota.CheckOriginQuota(origin, 100));
    }
  }
}
