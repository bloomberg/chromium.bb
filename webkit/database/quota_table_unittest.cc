// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/quota_table.h"

namespace {

class TestErrorDelegate : public sql::ErrorDelegate {
 public:
  virtual ~TestErrorDelegate() { }
  virtual int OnError(
      int error, sql::Connection* connection, sql::Statement* stmt) {
    return error;
  }
};

}  // namespace

namespace webkit_database {

static bool QuotaTableIsEmpty(sql::Connection* db) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM Quota"));
  return (statement.is_valid() && statement.Step() && !statement.ColumnInt(0));
}

TEST(QuotaTableTest, TestIt) {
  // Initialize the 'Quota' table.
  sql::Connection db;

  // Set an error delegate that will make all operations return false on error.
  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate());
  db.set_error_delegate(error_delegate);

  // Initialize the temp dir and the 'Databases' table.
  EXPECT_TRUE(db.OpenInMemory());
  QuotaTable quota_table(&db);
  EXPECT_TRUE(quota_table.Init());

  // The 'Quota' table should be empty.
  EXPECT_TRUE(QuotaTableIsEmpty(&db));

  // Set and check the quota for a new origin.
  string16 origin = ASCIIToUTF16("origin");
  EXPECT_TRUE(quota_table.SetOriginQuota(origin, 1000));
  EXPECT_EQ(1000, quota_table.GetOriginQuota(origin));

  // Reset and check the quota for the same origin.
  EXPECT_TRUE(quota_table.SetOriginQuota(origin, 2000));
  EXPECT_EQ(2000, quota_table.GetOriginQuota(origin));

  // Clear the quota for an origin
  EXPECT_TRUE(quota_table.ClearOriginQuota(origin));
  EXPECT_TRUE(quota_table.GetOriginQuota(origin) < 0);

  // Check that there's no quota set for unknown origins.
  EXPECT_TRUE(quota_table.GetOriginQuota(ASCIIToUTF16("unknown_origin")) < 0);
}

}  // namespace webkit_database
