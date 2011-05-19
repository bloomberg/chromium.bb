// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <set>

#include "app/sql/connection.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class QuotaDatabaseTest : public testing::Test {
 protected:
  void LazyOpen(const FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    EXPECT_FALSE(db.LazyOpen(false));
    ASSERT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.db_.get());
    EXPECT_TRUE(kDbFile.empty() || file_util::PathExists(kDbFile));
  }

  void HostQuota(const FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const char* kHost = "foo.com";
    const int kQuota1 = 13579;
    const int kQuota2 = kQuota1 + 1024;

    int64 quota = -1;
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypePersistent, &quota));

    // Insert quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kStorageTypeTemporary, kQuota1));
    EXPECT_TRUE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_EQ(kQuota1, quota);

    // Update quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kStorageTypeTemporary, kQuota2));
    EXPECT_TRUE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_EQ(kQuota2, quota);

    // Quota for persistent must not be updated.
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypePersistent, &quota));

    // Delete temporary storage quota.
    EXPECT_TRUE(db.DeleteHostQuota(kHost, kStorageTypeTemporary));
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
  }

  void GlobalQuota(const FilePath& kDbFile) {
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

  void OriginLastAccessTimeLRU(const FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    std::set<GURL> exceptions;
    GURL origin;
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_TRUE(origin.is_empty());

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

    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_EQ(kOrigin1.spec(), origin.spec());

    exceptions.insert(kOrigin1);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_EQ(kOrigin2.spec(), origin.spec());

    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_EQ(kOrigin3.spec(), origin.spec());

    exceptions.insert(kOrigin3);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_TRUE(origin.is_empty());

    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin1, kStorageTypeTemporary, base::Time::Now()));

    // Delete origin/type last access time information.
    EXPECT_TRUE(db.DeleteOriginLastAccessTime(kOrigin3, kStorageTypeTemporary));

    // Querying again to see if the deletion has worked.
    exceptions.clear();
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_EQ(kOrigin2.spec(), origin.spec());

    exceptions.insert(kOrigin1);
    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions, &origin));
    EXPECT_TRUE(origin.is_empty());
  }

  void RegisterOrigins(const FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);

    const GURL kOrigins[] = {
      GURL("http://a/"),
      GURL("http://b/"),
      GURL("http://c/") };
    std::set<GURL> origins(kOrigins, kOrigins + ARRAYSIZE_UNSAFE(kOrigins));

    EXPECT_TRUE(db.RegisterOrigins(origins,
                                   kStorageTypeTemporary,
                                   base::Time()));

    int used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(0, used_count);

    EXPECT_TRUE(db.SetOriginLastAccessTime(
        GURL("http://a/"), kStorageTypeTemporary,
        base::Time::FromDoubleT(1.0)));
    used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(1, used_count);

    EXPECT_TRUE(db.RegisterOrigins(origins,
                                   kStorageTypeTemporary,
                                   base::Time()));

    used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(1, used_count);
  }
};

TEST_F(QuotaDatabaseTest, LazyOpen) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  LazyOpen(kDbFile);
  LazyOpen(FilePath());
}

TEST_F(QuotaDatabaseTest, HostQuota) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  HostQuota(kDbFile);
  HostQuota(FilePath());
}

TEST_F(QuotaDatabaseTest, GlobalQuota) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  GlobalQuota(kDbFile);
  GlobalQuota(FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastAccessTimeLRU) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  OriginLastAccessTimeLRU(kDbFile);
  OriginLastAccessTimeLRU(FilePath());
}

TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  QuotaDatabase db(kDbFile);

  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(true));
  EXPECT_TRUE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(false));
  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
}

TEST_F(QuotaDatabaseTest, RegisterOrigins) {
  ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const FilePath kDbFile = data_dir.path().AppendASCII("quota_manager.db");
  RegisterOrigins(kDbFile);
  RegisterOrigins(FilePath());
}
}  // namespace quota
