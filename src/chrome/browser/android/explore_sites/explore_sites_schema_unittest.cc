// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_schema.h"

#include <limits>
#include <memory>

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace explore_sites {

class ExploreSitesSchemaTest : public testing::Test {
 public:
  ExploreSitesSchemaTest() = default;
  ~ExploreSitesSchemaTest() override = default;

  void SetUp() override {
    db_ = std::make_unique<sql::Database>();
    ASSERT_TRUE(db_->OpenInMemory());
    ASSERT_FALSE(sql::MetaTable::DoesTableExist(db_.get()));
  }

  void CheckTablesExistence() {
    EXPECT_TRUE(db_->DoesTableExist("sites"));
    EXPECT_TRUE(db_->DoesTableExist("categories"));
    EXPECT_TRUE(db_->DoesTableExist("site_blacklist"));
  }

 protected:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<ExploreSitesSchema> schema_;
};

TEST_F(ExploreSitesSchemaTest, TestSchemaCreationFromNothing) {
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db_.get(), std::numeric_limits<int>::max(),
                              std::numeric_limits<int>::max()));
  EXPECT_EQ(ExploreSitesSchema::kCurrentVersion, meta_table.GetVersionNumber());
  EXPECT_EQ(ExploreSitesSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());
}

TEST_F(ExploreSitesSchemaTest, TestMissingTablesAreCreatedAtLatestVersion) {
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db_.get(), ExploreSitesSchema::kCurrentVersion,
                              ExploreSitesSchema::kCompatibleVersion));
  EXPECT_EQ(ExploreSitesSchema::kCurrentVersion, meta_table.GetVersionNumber());
  EXPECT_EQ(ExploreSitesSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());

  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
}

TEST_F(ExploreSitesSchemaTest, TestMissingTablesAreRecreated) {
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE sites"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE categories"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE site_blacklist"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
}

}  // namespace explore_sites
