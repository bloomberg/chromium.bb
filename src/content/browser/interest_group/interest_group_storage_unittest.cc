// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
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

// Test that joining and interest group twice increments the counter.
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
  GURL update_url("https://owner.example.com/group1Update");
  url::Origin test_origin = url::Origin::Create(update_url);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.update_url = update_url;
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // Add a little delay so groups are returned in a deterministic order.
  // Interest groups are returned in descending order based on expiration time.
  task_environment().FastForwardBy(base::Microseconds(1));

  InterestGroup g2 = NewInterestGroup(test_origin, "name2");
  g2.update_url = update_url;
  storage->JoinInterestGroup(g2, GURL("https://owner.example.com/join2"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_url, groups[0].update_url_kanon->key);
  EXPECT_EQ(0, groups[0].update_url_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[0].update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].update_url_kanon);
  EXPECT_EQ(update_url, groups[1].update_url_kanon->key);
  EXPECT_EQ(0, groups[1].update_url_kanon->k);
  EXPECT_EQ(base::Time::Min(), groups[1].update_url_kanon->last_updated);

  base::Time update_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData kanon{update_url, 10, update_time};
  storage->UpdateInterestGroupUpdateURLKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_url, groups[0].update_url_kanon->key);
  EXPECT_EQ(10, groups[0].update_url_kanon->k);
  EXPECT_EQ(update_time, groups[0].update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].update_url_kanon);
  EXPECT_EQ(update_url, groups[1].update_url_kanon->key);
  EXPECT_EQ(10, groups[1].update_url_kanon->k);
  EXPECT_EQ(update_time, groups[1].update_url_kanon->last_updated);

  task_environment().FastForwardBy(base::Seconds(1));

  update_time = base::Time::Now();
  kanon = StorageInterestGroup::KAnonymityData{update_url, 12, update_time};
  storage->UpdateInterestGroupUpdateURLKAnonymity(kanon, absl::nullopt);

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ("name2", groups[0].interest_group.name);
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_url, groups[0].update_url_kanon->key);
  EXPECT_EQ(12, groups[0].update_url_kanon->k);
  EXPECT_EQ(update_time, groups[0].update_url_kanon->last_updated);

  EXPECT_EQ("name", groups[1].interest_group.name);
  ASSERT_TRUE(groups[1].update_url_kanon);
  EXPECT_EQ(update_url, groups[1].update_url_kanon->key);
  EXPECT_EQ(12, groups[1].update_url_kanon->k);
  EXPECT_EQ(update_time, groups[1].update_url_kanon->last_updated);
}

TEST_F(InterestGroupStorageTest, UpdatesAdKAnonymity) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("http://owner.example.com/ad1");
  GURL ad2_url = GURL("http://owner.example.com/ad2");
  GURL ad3_url = GURL("http://owner.example.com/ad3");

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
  GURL update_url("https://owner.example.com/groupUpdate");
  url::Origin test_origin = url::Origin::Create(update_url);
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
  g.update_url = update_url;
  g.expiry = base::Time::Now() + base::Days(1);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // Update the k-anonymity data.
  base::Time update_kanon_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData ad1_kanon{ad1_url, 10,
                                                 update_kanon_time};
  StorageInterestGroup::KAnonymityData ad2_kanon{ad2_url, 20,
                                                 update_kanon_time};
  StorageInterestGroup::KAnonymityData update_kanon{update_url, 30,
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
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].update_url_kanon.value());
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
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].update_url_kanon.value());
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
  update_kanon = {update_url, 0, base::Time::Min()};
  name_kanon = {key, 0, base::Time::Min()};
  expected_output = {ad1_kanon, ad2_kanon};
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].ads_kanon,
              testing::UnorderedElementsAreArray(expected_output));
  ASSERT_TRUE(groups[0].update_url_kanon);
  EXPECT_EQ(update_kanon, groups[0].update_url_kanon.value());
  ASSERT_TRUE(groups[0].name_kanon);
  EXPECT_EQ(name_kanon, groups[0].name_kanon.value());
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  const url::Origin partial_origin =
      url::Origin::Create(GURL("https://partial.example.com"));
  InterestGroup partial = NewInterestGroup(partial_origin, "partial");
  const url::Origin full_origin =
      url::Origin::Create(GURL("https://full.example.com"));
  InterestGroup full;
  full.owner = full_origin;
  full.name = "full";
  full.expiry = base::Time::Now() + base::Days(30);
  full.bidding_url = GURL("https://full.example.com/bid");
  full.bidding_wasm_helper_url = GURL("https://full.example.com/bid_wasm");
  full.update_url = GURL("https://full.example.com/update");
  full.trusted_bidding_signals_url = GURL("https://full.example.com/signals");
  full.trusted_bidding_signals_keys =
      absl::make_optional(std::vector<std::string>{"a", "b", "c", "d"});
  full.user_bidding_signals = "foo";
  full.ads.emplace();
  full.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/ad1"), "metadata1"));
  full.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/ad2"), "metadata2"));
  full.ad_components.emplace();
  full.ad_components->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/adcomponent1"), "metadata1c"));
  full.ad_components->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/adcomponent2"), "metadata2c"));

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

}  // namespace content
