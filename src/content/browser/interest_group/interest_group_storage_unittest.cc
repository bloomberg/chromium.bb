// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <stddef.h>

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "net/base/escape.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

using blink::InterestGroup;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class InterestGroupStorageTest : public testing::Test {
 public:
  InterestGroupStorageTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kInterestGroupStorage,
        {{"max_owners", "10"},
         {"max_groups_per_owner", "10"},
         {"max_ops_before_maintenance", "100"}});
  }

  std::unique_ptr<InterestGroupStorage> CreateStorage() {
    return std::make_unique<InterestGroupStorage>(temp_directory_.GetPath());
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("InterestGroups"));
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  InterestGroup NewInterestGroup(url::Origin owner, std::string name) {
    InterestGroup result;
    result.owner = owner;
    result.name = name;
    result.expiry = base::Time::Now() + base::Days(30);
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_directory_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InterestGroupStorageTest, DatabaseInitialized_CreateDatabase) {
  EXPECT_FALSE(base::PathExists(db_path()));

  { std::unique_ptr<InterestGroupStorage> storage = CreateStorage(); }

  // InterestGroupStorageSqlImpl opens the database lazily.
  EXPECT_FALSE(base::PathExists(db_path()));

  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(test_origin, "example");
  }

  // InterestGroupStorage creates the database if it doesn't exist.
  EXPECT_TRUE(base::PathExists(db_path()));

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history], [kanon],
    // [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseRazesOldVersion) {
  ASSERT_FALSE(base::PathExists(db_path()));

  // Create an empty database with old schema version (version=1).
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta_table;
    EXPECT_TRUE(
        meta_table.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_path()));
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    // We need to perform an interest group operation to trigger DB
    // initialization.
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(test_origin, "example");
  }

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history], [kanon],
    // [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseRazesNewVersion) {
  ASSERT_FALSE(base::PathExists(db_path()));

  // Create an empty database with a newer schema version (version=1000000).
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&raw_db, /*version=*/1000000,
                                /*compatible_version=*/1000000));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_path()));
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    // We need to perform an interest group operation to trigger DB
    // initialization.
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(test_origin, "example");
  }

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history], [kanon],
    // [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseJoin) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  InterestGroup test_group = NewInterestGroup(test_origin, "example");
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    storage->JoinInterestGroup(test_group, test_origin.GetURL());
  }
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ(test_origin, origins[0]);
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetInterestGroupsForOwner(test_origin);
    EXPECT_EQ(1u, interest_groups.size());
    EXPECT_EQ(test_origin, interest_groups[0].interest_group.owner);
    EXPECT_EQ("example", interest_groups[0].interest_group.name);
    EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
    EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
  }
  histograms.ExpectUniqueSample("Storage.InterestGroup.PerSiteCount", 1u, 1);
}

// Test that joining an interest group twice increments the counter.
// Test that joining multiple interest groups with the same owner only creates a
// single distinct owner. Test that leaving one interest group does not affect
// membership of other interest groups by the same owner.
TEST_F(InterestGroupStorageTest, JoinJoinLeave) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example2"),
                             test_origin.GetURL());

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(2u, interest_groups.size());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  storage->LeaveInterestGroup(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example2", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);
}

// Join 5 interest groups in the same origin, and one interest group in another
// origin.
//
// Fetch interest groups for update with a limit of 2 interest groups. Only 2
// interest groups should be returned, and they should all belong to the first
// test origin.
//
// Then, fetch 100 groups for update. Only 5 should be returned.
TEST_F(InterestGroupStorageTest, GetInterestGroupsForUpdate) {
  const url::Origin test_origin1 =
      url::Origin::Create(GURL("https://owner1.example.com"));
  const url::Origin test_origin2 =
      url::Origin::Create(GURL("https://owner2.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  constexpr size_t kNumOrigin1Groups = 5, kSmallFetchGroups = 2,
                   kLargeFetchGroups = 100;
  ASSERT_LT(kSmallFetchGroups, kNumOrigin1Groups);
  ASSERT_GT(kLargeFetchGroups, kNumOrigin1Groups);
  for (size_t i = 0; i < kNumOrigin1Groups; i++) {
    storage->JoinInterestGroup(
        NewInterestGroup(test_origin1,
                         base::StrCat({"example", base::NumberToString(i)})),
        test_origin1.GetURL());
  }
  storage->JoinInterestGroup(NewInterestGroup(test_origin2, "example"),
                             test_origin2.GetURL());

  std::vector<StorageInterestGroup> update_groups =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kSmallFetchGroups);

  EXPECT_EQ(kSmallFetchGroups, update_groups.size());
  for (const auto& group : update_groups) {
    EXPECT_EQ(test_origin1, group.interest_group.owner);
  }

  update_groups =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kLargeFetchGroups);

  EXPECT_EQ(kNumOrigin1Groups, update_groups.size());
  for (const auto& group : update_groups) {
    EXPECT_EQ(test_origin1, group.interest_group.owner);
  }
}

TEST_F(InterestGroupStorageTest, BidCount) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->bid_count);
}

TEST_F(InterestGroupStorageTest, RecordsWins) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const GURL ad1_url = GURL("http://owner.example.com/ad1");
  const GURL ad2_url = GURL("http://owner.example.com/ad2");
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  std::string ad1_json = "{url: '" + ad1_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad1_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->bid_count);

  // Add the second win *after* the first so we can check ordering.
  task_environment().FastForwardBy(base::Seconds(1));
  std::string ad2_json = "{url: '" + ad2_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad2_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->bid_count);
  EXPECT_EQ(2u, interest_groups[0].bidding_browser_signals->prev_wins.size());
  // Ad wins should be listed in reverse chronological order.
  EXPECT_EQ(ad2_json,
            interest_groups[0].bidding_browser_signals->prev_wins[0]->ad_json);
  EXPECT_EQ(ad1_json,
            interest_groups[0].bidding_browser_signals->prev_wins[1]->ad_json);

  // Try delete
  storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&test_origin](const url::Origin& candidate) {
        return candidate == test_origin;
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

// Test updating k-anonymity data for interest group names. Uses two Interest
// Groups with similar names, which should not share k-anonymity data, to
// verify database key creation works correctly.
TEST_F(InterestGroupStorageTest, UpdatesInterestGroupNameKAnonymity) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const std::string name = "name with space";
  const std::string name2 = "name%20with%20space";

  const GURL key = test_origin.GetURL().Resolve(net::EscapePath(name));
  const GURL key2 = test_origin.GetURL().Resolve(net::EscapePath(name2));

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  storage->JoinInterestGroup(NewInterestGroup(test_origin, name),
                             GURL("https://owner.example.com/join"));
  // Add a little delay so groups are returned in a deterministic order.
  // Interest groups are returned in descending order based on expiration time.
  task_environment().FastForwardBy(base::Microseconds(1));
  storage->JoinInterestGroup(NewInterestGroup(test_origin, name2),
                             GURL("https://owner.example.com/join"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ(name, groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].name_kanon);
  EXPECT_EQ(key, groups[1].name_kanon->key);
  EXPECT_EQ(0, groups[1].name_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[1].name_kanon->last_updated);

  EXPECT_EQ(name2, groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(key2, groups[0].name_kanon->key);
  EXPECT_EQ(0, groups[0].name_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[0].name_kanon->last_updated);

  base::Time update_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData kanon{key, 10, update_time};
  storage->UpdateInterestGroupNameKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ(name, groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].name_kanon);
  EXPECT_EQ(key, groups[1].name_kanon->key);
  EXPECT_EQ(10, groups[1].name_kanon->k);
  EXPECT_EQ(update_time, groups[1].name_kanon->last_updated);

  EXPECT_EQ(name2, groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(key2, groups[0].name_kanon->key);
  EXPECT_EQ(0, groups[0].name_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[0].name_kanon->last_updated);

  task_environment().FastForwardBy(base::Seconds(1));

  update_time = base::Time::Now();
  kanon = StorageInterestGroup::KAnonymityData{key, 12, update_time};
  storage->UpdateInterestGroupNameKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ(name, groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].name_kanon);
  EXPECT_EQ(key, groups[1].name_kanon->key);
  EXPECT_EQ(12, groups[1].name_kanon->k);
  EXPECT_EQ(update_time, groups[1].name_kanon->last_updated);

  EXPECT_EQ(name2, groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(key2, groups[0].name_kanon->key);
  EXPECT_EQ(0, groups[0].name_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[0].name_kanon->last_updated);
}

// Tests updating the Interest Group Update URL k-anonymity data works and
// is shared for both interest groups if they share the same update URL.
TEST_F(InterestGroupStorageTest, UpdatesInterestGroupUpdateURLKAnonymity) {
  GURL daily_update_url("https://owner.example.com/group1Update");
  url::Origin test_origin = url::Origin::Create(daily_update_url);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.daily_update_url = daily_update_url;
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // Add a little delay so groups are returned in a deterministic order.
  // Interest groups are returned in descending order based on expiration time.
  task_environment().FastForwardBy(base::Microseconds(1));

  InterestGroup g2 = NewInterestGroup(test_origin, "name2");
  g2.daily_update_url = daily_update_url;
  storage->JoinInterestGroup(g2, GURL("https://owner.example.com/join2"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[0].daily_update_url_kanon->key);
  EXPECT_EQ(0, groups[0].daily_update_url_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[0].daily_update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[1].daily_update_url_kanon->key);
  EXPECT_EQ(0, groups[1].daily_update_url_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[1].daily_update_url_kanon->last_updated);

  base::Time update_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData kanon{daily_update_url, 10, update_time};
  storage->UpdateInterestGroupUpdateURLKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[0].daily_update_url_kanon->key);
  EXPECT_EQ(10, groups[0].daily_update_url_kanon->k);
  EXPECT_EQ(update_time, groups[0].daily_update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[1].daily_update_url_kanon->key);
  EXPECT_EQ(10, groups[1].daily_update_url_kanon->k);
  EXPECT_EQ(update_time, groups[1].daily_update_url_kanon->last_updated);

  task_environment().FastForwardBy(base::Seconds(1));

  update_time = base::Time::Now();
  kanon =
      StorageInterestGroup::KAnonymityData{daily_update_url, 12, update_time};
  storage->UpdateInterestGroupUpdateURLKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[0].daily_update_url_kanon->key);
  EXPECT_EQ(12, groups[0].daily_update_url_kanon->k);
  EXPECT_EQ(update_time, groups[0].daily_update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].daily_update_url_kanon);
  EXPECT_EQ(daily_update_url, groups[1].daily_update_url_kanon->key);
  EXPECT_EQ(12, groups[1].daily_update_url_kanon->k);
  EXPECT_EQ(update_time, groups[1].daily_update_url_kanon->last_updated);
}

TEST_F(InterestGroupStorageTest, UpdatesAdKAnonymity) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ads->push_back(blink::InterestGroup::Ad(ad2_url, "metadata2"));
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad1_url, "component_metadata1"));
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad3_url, "component_metadata3"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  std::vector<StorageInterestGroup::KAnonymityData> expected_output = {
      {ad1_url, 0, base::Time::Min()},
      {ad2_url, 0, base::Time::Min()},
      {ad1_url, 0, base::Time::Min()},
      {ad3_url, 0, base::Time::Min()},
  };

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));

  base::Time update_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData kanon{ad1_url, 10, update_time};
  storage->UpdateAdKAnonymity(kanon, absl::nullopt);
  expected_output[0] = kanon;
  expected_output[2] = kanon;

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));

  task_environment().FastForwardBy(base::Seconds(1));

  update_time = base::Time::Now();
  kanon = StorageInterestGroup::KAnonymityData{ad2_url, 12, update_time};
  storage->UpdateAdKAnonymity(kanon, absl::nullopt);
  expected_output[1] = kanon;

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));
}

TEST_F(InterestGroupStorageTest, KAnonDataExpires) {
  GURL daily_update_url("https://owner.example.com/groupUpdate");
  url::Origin test_origin = url::Origin::Create(daily_update_url);
  const std::string name = "name";
  const GURL key = test_origin.GetURL().Resolve(net::EscapePath(name));
  // We make the ad urls equal to the name key and update urls to verify the
  // database stores them separately.
  GURL ad1_url = GURL("https://owner.example.com/groupUpdate");
  GURL ad2_url = GURL("https://owner.example.com/name");

  InterestGroup g = NewInterestGroup(test_origin, name);
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad2_url, "component_metadata2"));
  g.daily_update_url = daily_update_url;
  g.expiry = base::Time::Now() + base::Days(1);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // Update the k-anonymity data.
  base::Time update_kanon_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData ad1_kanon{ad1_url, 10,
                                                 update_kanon_time};
  StorageInterestGroup::KAnonymityData ad2_kanon{ad2_url, 20,
                                                 update_kanon_time};
  StorageInterestGroup::KAnonymityData update_kanon{daily_update_url, 30,
                                                    update_kanon_time};
  StorageInterestGroup::KAnonymityData name_kanon{key, 40, update_kanon_time};
  storage->UpdateAdKAnonymity(ad1_kanon, update_kanon_time);
  storage->UpdateAdKAnonymity(ad2_kanon, update_kanon_time);
  storage->UpdateInterestGroupUpdateURLKAnonymity(update_kanon,
                                                  update_kanon_time);
  storage->UpdateInterestGroupNameKAnonymity(name_kanon, update_kanon_time);

  // Check k-anonymity data was correctly set.
  std::vector<StorageInterestGroup::KAnonymityData> expected_output = {
      ad1_kanon, ad2_kanon};
  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].daily_update_url_kanon.value());
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(name_kanon, groups[0].name_kanon.value());

  // Fast-forward past interest group expiration.
  task_environment().FastForwardBy(base::Days(2));

  // Interest group should no longer exist.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(0u, groups.size());

  // Join again and expect the same kanon values.
  g.expiry = base::Time::Now() + base::Days(1);
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join3"));

  // K-anon data should still be the same.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].daily_update_url_kanon.value());
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(name_kanon, groups[0].name_kanon.value());

  // Fast-forward past interest group and kanon value expiration.
  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength);

  // Interest group should no longer exist.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(0u, groups.size());

  // Join again and expect the default kanon values.
  g.expiry = base::Time::Now() + base::Days(1);
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join3"));

  // K-anon data should be the default.
  ad1_kanon = {ad1_url, 0, base::Time::Min()};
  ad2_kanon = {ad2_url, 0, base::Time::Min()};
  update_kanon = {daily_update_url, 0, base::Time::Min()};
  name_kanon = {key, 0, base::Time::Min()};
  expected_output = {ad1_kanon, ad2_kanon};
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));
  ASSERT_TRUE(groups[0].daily_update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].daily_update_url_kanon.value());
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(name_kanon, groups[0].name_kanon.value());
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  const url::Origin partial_origin =
      url::Origin::Create(GURL("https://partial.example.com"));
  InterestGroup partial = NewInterestGroup(partial_origin, "partial");
  const url::Origin full_origin =
      url::Origin::Create(GURL("https://full.example.com"));
  InterestGroup full(
      /*expiry=*/base::Time::Now() + base::Days(30), /*owner=*/full_origin,
      /*name=*/"full", /*priority=*/1.0,
      /*bidding_url=*/GURL("https://full.example.com/bid"),
      /*bidding_wasm_helper_url=*/GURL("https://full.example.com/bid_wasm"),
      /*daily_update_url=*/GURL("https://full.example.com/update"),
      /*trusted_bidding_signals_url=*/GURL("https://full.example.com/signals"),
      /*trusted_bidding_signals_keys=*/
      std::vector<std::string>{"a", "b", "c", "d"},
      /*user_bidding_signals=*/"foo",
      /*ads=*/
      std::vector<InterestGroup::Ad>{
          blink::InterestGroup::Ad(GURL("https://full.example.com/ad1"),
                                   "metadata1"),
          blink::InterestGroup::Ad(GURL("https://full.example.com/ad2"),
                                   "metadata2")},
      /*ad_components=*/
      std::vector<InterestGroup::Ad>{
          blink::InterestGroup::Ad(
              GURL("https://full.example.com/adcomponent1"), "metadata1c"),
          blink::InterestGroup::Ad(
              GURL("https://full.example.com/adcomponent2"), "metadata2c")});

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(partial, partial_origin.GetURL());
  storage->JoinInterestGroup(full, full_origin.GetURL());

  std::vector<StorageInterestGroup> storage_interest_groups =
      storage->GetInterestGroupsForOwner(partial_origin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_TRUE(
      partial.IsEqualForTesting(storage_interest_groups[0].interest_group));

  storage_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_TRUE(
      full.IsEqualForTesting(storage_interest_groups[0].interest_group));

  // Test update as well.
  InterestGroup updated = full;
  updated.bidding_url = GURL("https://full.example.com/bid2");
  updated.bidding_wasm_helper_url = GURL("https://full.example.com/bid_wasm2");
  updated.trusted_bidding_signals_url =
      GURL("https://full.example.com/signals2");
  updated.trusted_bidding_signals_keys =
      absl::make_optional(std::vector<std::string>{"a", "b2", "c", "d"});
  updated.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/ad3"), "metadata3"));
  updated.ad_components->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/adcomponent3"), "metadata3c"));
  storage->UpdateInterestGroup(updated);

  storage_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_TRUE(
      updated.IsEqualForTesting(storage_interest_groups[0].interest_group));
}

TEST_F(InterestGroupStorageTest, DeleteOriginDeleteAll) {
  const url::Origin owner_originA =
      url::Origin::Create(GURL("https://owner.example.com"));
  const url::Origin owner_originB =
      url::Origin::Create(GURL("https://owner2.example.com"));
  const url::Origin owner_originC =
      url::Origin::Create(GURL("https://owner3.example.com"));
  const url::Origin joining_originA =
      url::Origin::Create(GURL("https://joinerA.example.com"));
  const url::Origin joining_originB =
      url::Origin::Create(GURL("https://joinerB.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  storage->JoinInterestGroup(NewInterestGroup(owner_originA, "example"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "example"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originC, "example"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "exampleB"),
                             joining_originB.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originA, owner_originB,
                                            owner_originC));
  std::vector<url::Origin> joining_origins =
      storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins,
              UnorderedElementsAre(joining_originA, joining_originB));

  storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&owner_originA](const url::Origin& origin) {
        return origin == owner_originA;
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originB, owner_originC));
  joining_origins = storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins,
              UnorderedElementsAre(joining_originA, joining_originB));

  // Delete all interest groups that joined on joining_origin A. We expect that
  // we will be left with the one that joined on joining_origin B.
  storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&joining_originA](const url::Origin& origin) {
        return origin == joining_originA;
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originB));
  joining_origins = storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins, UnorderedElementsAre(joining_originB));

  storage->DeleteInterestGroupData({});

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

// Maintenance should prune the number of interest groups and interest group
// owners based on the set limit.
TEST_F(InterestGroupStorageTest, JoinTooManyGroupNames) {
  base::HistogramTester histograms;
  const size_t kExcessOwners = 10;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const size_t max_groups_per_owner =
      blink::features::kInterestGroupStorageMaxGroupsPerOwner.Get();
  const size_t num_groups = max_groups_per_owner + kExcessOwners;
  std::vector<std::string> added_groups;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < num_groups; i++) {
    const std::string group_name = base::NumberToString(i);
    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));

    storage->JoinInterestGroup(NewInterestGroup(test_origin, group_name),
                               test_origin.GetURL());
    added_groups.push_back(group_name);
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(num_groups, interest_groups.size());
  histograms.ExpectBucketCount("Storage.InterestGroup.PerSiteCount", num_groups,
                               1);

  // Allow enough idle time to trigger maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                   base::Seconds(1));

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(max_groups_per_owner, interest_groups.size());
  histograms.ExpectBucketCount("Storage.InterestGroup.PerSiteCount",
                               max_groups_per_owner, 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.PerSiteCount", 2);

  std::vector<std::string> remaining_groups;
  for (const auto& db_group : interest_groups) {
    remaining_groups.push_back(db_group.interest_group.name);
  }
  std::vector<std::string> remaining_groups_expected(
      added_groups.begin() + kExcessOwners, added_groups.end());
  EXPECT_THAT(remaining_groups,
              UnorderedElementsAreArray(remaining_groups_expected));
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 1);
}

// Excess group owners should have their groups pruned by maintenance.
// In this test we trigger maintenance by having too many operations in a short
// period to test max_ops_before_maintenance_.
TEST_F(InterestGroupStorageTest, JoinTooManyGroupOwners) {
  const size_t kExcessGroups = 10;
  const size_t max_owners =
      blink::features::kInterestGroupStorageMaxOwners.Get();
  const size_t max_ops =
      blink::features::kInterestGroupStorageMaxOpsBeforeMaintenance.Get();
  const size_t num_groups = max_owners + kExcessGroups;
  std::vector<url::Origin> added_origins;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < num_groups; i++) {
    const url::Origin test_origin = url::Origin::Create(GURL(
        base::StrCat({"https://", base::NumberToString(i), ".example.com/"})));

    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));

    storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                               test_origin.GetURL());
    added_origins.push_back(test_origin);
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAreArray(added_origins));

  // Perform enough operations to trigger maintenance, without passing time.
  for (size_t i = 0; i < max_ops; i++) {
    // Any read-only operation will work here. This one should be fast.
    origins = storage->GetAllInterestGroupOwners();
  }

  // The oldest few interest groups should have been cleared during maintenance.
  origins = storage->GetAllInterestGroupOwners();
  std::vector<url::Origin> remaining_origins_expected(
      added_origins.begin() + kExcessGroups, added_origins.end());
  EXPECT_THAT(origins, UnorderedElementsAreArray(remaining_origins_expected));
}

TEST_F(InterestGroupStorageTest, DBMaintenanceExpiresOldInterestGroups) {
  base::HistogramTester histograms;

  const url::Origin keep_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::vector<::url::Origin> test_origins = {
      url::Origin::Create(GURL("https://owner.example.com")),
      url::Origin::Create(GURL("https://owner2.example.com")),
      url::Origin::Create(GURL("https://owner3.example.com")),
  };

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  base::Time original_maintenance_time =
      storage->GetLastMaintenanceTimeForTesting();
  EXPECT_EQ(base::Time::Min(), original_maintenance_time);

  storage->JoinInterestGroup(NewInterestGroup(keep_origin, "keep"),
                             keep_origin.GetURL());
  for (const auto& origin : test_origins)
    storage->JoinInterestGroup(NewInterestGroup(origin, "discard"),
                               origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());
  base::Time next_maintenance_time =
      base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod -
                                   base::Seconds(1));

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(base::Seconds(2));
  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 1);

  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength -
                                   base::Days(1));
  // Verify that maintenance has not run. It's been long enough, but we haven't
  // made any calls.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  storage->JoinInterestGroup(NewInterestGroup(keep_origin, "keep"),
                             keep_origin.GetURL());
  next_maintenance_time = base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  interest_groups = storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());

  //  Maintenance should not have run since we have not been idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  // Advance past expiration and check that since DB maintenance last ran before
  // the expiration the outdated entries are present but not reported.
  task_environment().FastForwardBy(base::Days(1) + base::Seconds(1));

  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 2);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 2);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
  next_maintenance_time = base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  // All the groups should still be in the database since they shouldn't have
  // been cleaned up yet.
  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(4u, interest_groups.size());

  // Wait an hour to perform DB maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kMaintenanceInterval);

  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 3);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 3);

  // Verify that the database only contains unexpired entries.
  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
}

// Upgrades a v6 database dump to an expected v7 database.
// The v6 database dump was extracted from the InterestGroups database in
// a browser profile by using `sqlite3 dump <path-to-database>` and then
// cleaning up and formatting the output.
TEST_F(InterestGroupStorageTest, UpgradeFromV6) {
  // Create V6 database from dump
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path =
      file_path.AppendASCII("content/test/data/interest_group/schemaV6.sql");
  ASSERT_TRUE(base::PathExists(file_path));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));

  auto expected_interest_group_matcher = testing::UnorderedElementsAre(
      testing::AllOf(
          testing::Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  testing::Field("expiry", &blink::InterestGroup::expiry,
                                 base::Time::FromDeltaSinceWindowsEpoch(
                                     base::Microseconds(13293932603076872))),
                  testing::Field(
                      "owner", &blink::InterestGroup::owner,
                      url::Origin::Create(GURL("https://owner.example.com"))),
                  testing::Field("name", &blink::InterestGroup::name, "group1"),
                  testing::Field("priority", &blink::InterestGroup::priority,
                                 0.0),
                  testing::Field("bidding_url",
                                 &blink::InterestGroup::bidding_url,
                                 GURL("https://owner.example.com/bidder.js")),
                  testing::Field("bidding_wasm_helper_url",
                                 &blink::InterestGroup::bidding_wasm_helper_url,
                                 absl::nullopt),
                  testing::Field("daily_update_url",
                                 &blink::InterestGroup::daily_update_url,
                                 GURL("https://owner.example.com/update")),
                  testing::Field(
                      "trusted_bidding_signals_url",
                      &blink::InterestGroup::trusted_bidding_signals_url,
                      GURL("https://owner.example.com/signals")),
                  testing::Field(
                      "trusted_bidding_signals_keys",
                      &blink::InterestGroup::trusted_bidding_signals_keys,
                      std::vector<std::string>{"group1"}),
                  testing::Field("user_bidding_signals",
                                 &blink::InterestGroup::user_bidding_signals,
                                 "[[\"1\",\"2\"]]"),
                  testing::Field(
                      "ads", &blink::InterestGroup::ads,
                      testing::Property(
                          "value()",
                          &absl::optional<
                              std::vector<blink::InterestGroup::Ad>>::value,
                          testing::ElementsAre(testing::AllOf(
                              testing::Field(
                                  "render_url",
                                  &blink::InterestGroup::Ad::render_url,
                                  GURL("https://ads.example.com/1")),
                              testing::Field(
                                  "metadata",
                                  &blink::InterestGroup::Ad::metadata,
                                  "[\"4\",\"5\",null,\"6\"]"))))),
                  testing::Field("ad_components",
                                 &blink::InterestGroup::ad_components,
                                 absl::nullopt))),
          testing::Field("name_kanon", &StorageInterestGroup::name_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/group1"), 0,
                             base::Time::Min()}),
          testing::Field("daily_update_url_kanon",
                         &StorageInterestGroup::daily_update_url_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/update"), 0,
                             base::Time::Min()}),
          testing::Field("ads_kanon", &StorageInterestGroup::ads_kanon,
                         testing::UnorderedElementsAre(
                             StorageInterestGroup::KAnonymityData{
                                 GURL("https://ads.example.com/1"), 0,
                                 base::Time::Min()})),
          testing::Field(
              "joining_origin", &StorageInterestGroup::joining_origin,
              url::Origin::Create(GURL("https://publisher.example.com")))),
      testing::AllOf(
          testing::Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  testing::Field("expiry", &blink::InterestGroup::expiry,
                                 base::Time::FromDeltaSinceWindowsEpoch(
                                     base::Microseconds(13293932603080090))),
                  testing::Field(
                      "owner", &blink::InterestGroup::owner,
                      url::Origin::Create(GURL("https://owner.example.com"))),
                  testing::Field("name", &blink::InterestGroup::name, "group2"),
                  testing::Field("priority", &blink::InterestGroup::priority,
                                 0.0),
                  testing::Field("bidding_url",
                                 &blink::InterestGroup::bidding_url,
                                 GURL("https://owner.example.com/bidder.js")),
                  testing::Field("bidding_wasm_helper_url",
                                 &blink::InterestGroup::bidding_wasm_helper_url,
                                 absl::nullopt),
                  testing::Field("daily_update_url",
                                 &blink::InterestGroup::daily_update_url,
                                 GURL("https://owner.example.com/update")),
                  testing::Field(
                      "trusted_bidding_signals_url",
                      &blink::InterestGroup::trusted_bidding_signals_url,
                      GURL("https://owner.example.com/signals")),
                  testing::Field(
                      "trusted_bidding_signals_keys",
                      &blink::InterestGroup::trusted_bidding_signals_keys,
                      std::vector<std::string>{"group2"}),
                  testing::Field("user_bidding_signals",
                                 &blink::InterestGroup::user_bidding_signals,
                                 "[[\"1\",\"3\"]]"),
                  testing::Field(
                      "ads", &blink::InterestGroup::ads,
                      testing::Property(
                          "value()",
                          &absl::optional<
                              std::vector<blink::InterestGroup::Ad>>::value,
                          testing::ElementsAre(testing::AllOf(
                              testing::Field(
                                  "render_url",
                                  &blink::InterestGroup::Ad::render_url,
                                  GURL("https://ads.example.com/1")),
                              testing::Field(
                                  "metadata",
                                  &blink::InterestGroup::Ad::metadata,
                                  "[\"4\",\"5\",null,\"6\"]"))))),
                  testing::Field("ad_components",
                                 &blink::InterestGroup::ad_components,
                                 absl::nullopt))),
          testing::Field("name_kanon", &StorageInterestGroup::name_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/group2"), 0,
                             base::Time::Min()}),
          testing::Field("daily_update_url_kanon",
                         &StorageInterestGroup::daily_update_url_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/update"), 0,
                             base::Time::Min()}),
          testing::Field("ads_kanon", &StorageInterestGroup::ads_kanon,
                         testing::UnorderedElementsAre(
                             StorageInterestGroup::KAnonymityData{
                                 GURL("https://ads.example.com/1"), 0,
                                 base::Time::Min()})),
          testing::Field(
              "joining_origin", &StorageInterestGroup::joining_origin,
              url::Origin::Create(GURL("https://publisher.example.com")))),
      testing::AllOf(
          testing::Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  testing::Field("expiry", &blink::InterestGroup::expiry,
                                 base::Time::FromDeltaSinceWindowsEpoch(
                                     base::Microseconds(13293932603052561))),
                  testing::Field(
                      "owner", &blink::InterestGroup::owner,
                      url::Origin::Create(GURL("https://owner.example.com"))),
                  testing::Field("name", &blink::InterestGroup::name, "group3"),
                  testing::Field("priority", &blink::InterestGroup::priority,
                                 0.0),
                  testing::Field("bidding_url",
                                 &blink::InterestGroup::bidding_url,
                                 GURL("https://owner.example.com/bidder.js")),
                  testing::Field("bidding_wasm_helper_url",
                                 &blink::InterestGroup::bidding_wasm_helper_url,
                                 absl::nullopt),
                  testing::Field("daily_update_url",
                                 &blink::InterestGroup::daily_update_url,
                                 GURL("https://owner.example.com/update")),
                  testing::Field(
                      "trusted_bidding_signals_url",
                      &blink::InterestGroup::trusted_bidding_signals_url,
                      GURL("https://owner.example.com/signals")),
                  testing::Field(
                      "trusted_bidding_signals_keys",
                      &blink::InterestGroup::trusted_bidding_signals_keys,
                      std::vector<std::string>{"group3"}),
                  testing::Field("user_bidding_signals",
                                 &blink::InterestGroup::user_bidding_signals,
                                 "[[\"3\",\"2\"]]"),
                  testing::Field(
                      "ads", &blink::InterestGroup::ads,
                      testing::Property(
                          "value()",
                          &absl::optional<
                              std::vector<blink::InterestGroup::Ad>>::value,
                          testing::ElementsAre(testing::AllOf(
                              testing::Field(
                                  "render_url",
                                  &blink::InterestGroup::Ad::render_url,
                                  GURL("https://ads.example.com/1")),
                              testing::Field(
                                  "metadata",
                                  &blink::InterestGroup::Ad::metadata,
                                  "[\"4\",\"5\",null,\"6\"]"))))),
                  testing::Field("ad_components",
                                 &blink::InterestGroup::ad_components,
                                 absl::nullopt))),
          testing::Field("name_kanon", &StorageInterestGroup::name_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/group3"), 0,
                             base::Time::Min()}),
          testing::Field("daily_update_url_kanon",
                         &StorageInterestGroup::daily_update_url_kanon,
                         StorageInterestGroup::KAnonymityData{
                             GURL("https://owner.example.com/update"), 0,
                             base::Time::Min()}),
          testing::Field("ads_kanon", &StorageInterestGroup::ads_kanon,
                         testing::UnorderedElementsAre(
                             StorageInterestGroup::KAnonymityData{
                                 GURL("https://ads.example.com/1"), 0,
                                 base::Time::Min()})),
          testing::Field(
              "joining_origin", &StorageInterestGroup::joining_origin,
              url::Origin::Create(GURL("https://publisher.example.com")))));

  // Upgrade if necessary and read.
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    ASSERT_TRUE(storage);

    std::vector<StorageInterestGroup> interest_groups =
        storage->GetAllInterestGroupsUnfilteredForTesting();

    EXPECT_THAT(interest_groups, expected_interest_group_matcher);
  }

  // Make sure the database still works if we open it again.
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetAllInterestGroupsUnfilteredForTesting();

    EXPECT_THAT(interest_groups, expected_interest_group_matcher);
  }
}

}  // namespace content
