// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "app/sql/connection.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_database.h"

namespace {

const base::Time kZeroTime;

class TestErrorDelegate : public sql::ErrorDelegate {
 public:
  virtual ~TestErrorDelegate() { }
  virtual int OnError(
      int error, sql::Connection* connection, sql::Statement* stmt) {
    return error;
  }
};

}  // namespace

namespace quota {

TEST(QuotaDatabaseTest, LazyOpen) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  QuotaDatabase db(kDbFile);
  EXPECT_FALSE(db.LazyOpen(false));
  ASSERT_TRUE(db.LazyOpen(true));
  EXPECT_TRUE(file_util::PathExists(kDbFile));
}

TEST(QuotaDatabaseTest, OriginQuota) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  QuotaDatabase db(kDbFile);
  ASSERT_TRUE(db.LazyOpen(true));

  const GURL kOrigin("http://foo.com:8080/");
  const int kQuota1 = 13579;
  const int kQuota2 = kQuota1 + 1024;

  int64 quota = -1;
  EXPECT_FALSE(db.GetOriginQuota(kOrigin, kStorageTypeTemporary, &quota));
  EXPECT_FALSE(db.GetOriginQuota(kOrigin, kStorageTypePersistent, &quota));

  // Insert quota for temporary.
  EXPECT_TRUE(db.SetOriginQuota(kOrigin, kStorageTypeTemporary, kQuota1));
  EXPECT_TRUE(db.GetOriginQuota(kOrigin, kStorageTypeTemporary, &quota));
  EXPECT_EQ(kQuota1, quota);

  // Update quota for temporary.
  EXPECT_TRUE(db.SetOriginQuota(kOrigin, kStorageTypeTemporary, kQuota2));
  EXPECT_TRUE(db.GetOriginQuota(kOrigin, kStorageTypeTemporary, &quota));
  EXPECT_EQ(kQuota2, quota);

  // Quota for persistent must not be updated.
  EXPECT_FALSE(db.GetOriginQuota(kOrigin, kStorageTypePersistent, &quota));

  // Delete temporary storage info.
  EXPECT_TRUE(db.DeleteStorageInfo(kOrigin, kStorageTypeTemporary));
  EXPECT_FALSE(db.GetOriginQuota(kOrigin, kStorageTypeTemporary, &quota));
}

TEST(QuotaDatabaseTest, GlobalQuota) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  QuotaDatabase db(kDbFile);
  ASSERT_TRUE(db.LazyOpen(true));

  const int kQuota1 = 9999;
  const int kQuota2 = 86420;

  int64 quota = -1;
  EXPECT_FALSE(db.GetGlobalQuota(kStorageTypeTemporary, &quota));
  EXPECT_FALSE(db.GetGlobalQuota(kStorageTypePersistent, &quota));

  EXPECT_TRUE(db.SetGlobalQuota(kStorageTypeTemporary, kQuota1));
  EXPECT_TRUE(db.GetGlobalQuota(kStorageTypeTemporary, &quota));
  EXPECT_EQ(kQuota1, quota);

  EXPECT_TRUE(db.SetGlobalQuota(kStorageTypeTemporary, kQuota1 + 1024));
  EXPECT_TRUE(db.GetGlobalQuota(kStorageTypeTemporary, &quota));
  EXPECT_EQ(kQuota1 + 1024, quota);

  EXPECT_FALSE(db.GetGlobalQuota(kStorageTypePersistent, &quota));

  EXPECT_TRUE(db.SetGlobalQuota(kStorageTypePersistent, kQuota2));
  EXPECT_TRUE(db.GetGlobalQuota(kStorageTypePersistent, &quota));
  EXPECT_EQ(kQuota2, quota);
}

TEST(QuotaDatabaseTest, OriginLastAccessTimeLRU) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  QuotaDatabase db(kDbFile);
  ASSERT_TRUE(db.LazyOpen(true));

  std::vector<GURL> origins;
  EXPECT_TRUE(db.GetLRUOrigins(kStorageTypeTemporary, &origins, -1, 10));
  EXPECT_EQ(0U, origins.size());

  const GURL kOrigin1("http://a/");
  const GURL kOrigin2("http://b/");
  const GURL kOrigin3("http://c/");
  const GURL kOrigin4("http://p/");

  // Adding three temporary storages, and
  EXPECT_TRUE(db.SetOriginLastAccessTime(
      kOrigin1, kStorageTypeTemporary, base::Time::FromInternalValue(10)));
  EXPECT_TRUE(db.SetOriginLastAccessTime(
      kOrigin2, kStorageTypeTemporary, base::Time::FromInternalValue(20)));
  EXPECT_TRUE(db.SetOriginLastAccessTime(
      kOrigin3, kStorageTypeTemporary, base::Time::FromInternalValue(30)));

  // one persistent.
  EXPECT_TRUE(db.SetOriginLastAccessTime(
      kOrigin4, kStorageTypePersistent, base::Time::FromInternalValue(40)));

  EXPECT_TRUE(db.GetLRUOrigins(kStorageTypeTemporary, &origins, 0, 10));

  ASSERT_EQ(3U, origins.size());
  EXPECT_EQ(kOrigin1.spec(), origins[0].spec());
  EXPECT_EQ(kOrigin2.spec(), origins[1].spec());
  EXPECT_EQ(kOrigin3.spec(), origins[2].spec());

  EXPECT_TRUE(db.SetOriginLastAccessTime(
      kOrigin1, kStorageTypeTemporary, base::Time::Now()));

  EXPECT_TRUE(db.GetLRUOrigins(kStorageTypeTemporary, &origins, 0, 10));

  // Now kOrigin1 has used_count=1, so it should not be in the returned list.
  ASSERT_EQ(2U, origins.size());
  EXPECT_EQ(kOrigin2.spec(), origins[0].spec());
  EXPECT_EQ(kOrigin3.spec(), origins[1].spec());

  // Query again without used_count condition.
  EXPECT_TRUE(db.GetLRUOrigins(kStorageTypeTemporary, &origins, -1, 10));

  // Now kOrigin1 must be returned as the newest one.
  ASSERT_EQ(3U, origins.size());
  EXPECT_EQ(kOrigin2.spec(), origins[0].spec());
  EXPECT_EQ(kOrigin3.spec(), origins[1].spec());
  EXPECT_EQ(kOrigin1.spec(), origins[2].spec());
}

}  // namespace quota
