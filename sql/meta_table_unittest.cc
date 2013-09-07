// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SQLMetaTableTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(temp_dir_.path().AppendASCII("SQLMetaTableTest.db")));
  }

  virtual void TearDown() {
    db_.Close();
  }

  sql::Connection& db() { return db_; }

 private:
  base::ScopedTempDir temp_dir_;
  sql::Connection db_;
};

TEST_F(SQLMetaTableTest, DoesTableExist) {
  EXPECT_FALSE(sql::MetaTable::DoesTableExist(&db()));

  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));
  }

  EXPECT_TRUE(sql::MetaTable::DoesTableExist(&db()));
}

TEST_F(SQLMetaTableTest, VersionNumber) {
  // Compatibility versions one less than the main versions to make
  // sure the values aren't being crossed with each other.
  const int kVersionFirst = 2;
  const int kCompatVersionFirst = kVersionFirst - 1;
  const int kVersionSecond = 4;
  const int kCompatVersionSecond = kVersionSecond - 1;
  const int kVersionThird = 6;
  const int kCompatVersionThird = kVersionThird - 1;

  // First Init() sets the version info as expected.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionFirst, kCompatVersionFirst));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());
  }

  // Second Init() does not change the version info.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionSecond, kCompatVersionSecond));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());

    meta_table.SetVersionNumber(kVersionSecond);
    meta_table.SetCompatibleVersionNumber(kCompatVersionSecond);
  }

  // Version info from Set*() calls is seen.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionThird, kCompatVersionThird));
    EXPECT_EQ(kVersionSecond, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionSecond, meta_table.GetCompatibleVersionNumber());
  }
}

TEST_F(SQLMetaTableTest, StringValue) {
  const char kKey[] = "String Key";
  const std::string kFirstValue("First Value");
  const std::string kSecondValue("Second Value");

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, IntValue) {
  const char kKey[] = "Int Key";
  const int kFirstValue = 17;
  const int kSecondValue = 23;

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, Int64Value) {
  const char kKey[] = "Int Key";
  const int64 kFirstValue = GG_LONGLONG(5000000017);
  const int64 kSecondValue = GG_LONGLONG(5000000023);

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64 value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64 value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64 value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, DeleteKey) {
  const char kKey[] = "String Key";
  const std::string kValue("String Value");

  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

  // Value isn't present.
  std::string value;
  EXPECT_FALSE(meta_table.GetValue(kKey, &value));

  // Now value is present.
  EXPECT_TRUE(meta_table.SetValue(kKey, kValue));
  EXPECT_TRUE(meta_table.GetValue(kKey, &value));
  EXPECT_EQ(kValue, value);

  // After delete value isn't present.
  EXPECT_TRUE(meta_table.DeleteKey(kKey));
  EXPECT_FALSE(meta_table.GetValue(kKey, &value));
}

}  // namespace
