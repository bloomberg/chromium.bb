// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class TestResult : public ChromeSearchResult {
 public:
  explicit TestResult(const std::string& id) { set_id(id); }
  ~TestResult() override {}

  // ChromeSearchResult:
  void Open(int event_flags) override {}
};

std::unique_ptr<TestResult> make_result(const std::string& id) {
  return std::make_unique<TestResult>(id);
}

Results make_results(std::vector<std::string> ids) {
  Results res;
  for (const std::string& id : ids) {
    res.push_back(make_result(id));
  }
  return res;
}

}  // namespace

class RemovedResultsRankerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }
  bool IsInitialized(const RemovedResultsRanker& ranker) {
    return ranker.initialized();
  }

  PersistentProto<RemovedResultsProto> GetProto() {
    return PersistentProto<RemovedResultsProto>(GetPath(), base::Seconds(0));
  }

  RemovedResultsProto ReadFromDisk() {
    EXPECT_TRUE(base::PathExists(GetPath()));
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    RemovedResultsProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

TEST_F(RemovedResultsRankerTest, CheckInitializeEmpty) {
  RemovedResultsRanker ranker(GetProto());
  EXPECT_FALSE(IsInitialized(ranker));
  Wait();

  EXPECT_TRUE(IsInitialized(ranker));
  RemovedResultsProto proto = ReadFromDisk();
  EXPECT_EQ(proto.removed_ids_size(), 0);
}

TEST_F(RemovedResultsRankerTest, RemoveResults) {
  RemovedResultsRanker ranker(GetProto());
  Wait();

  // Request to remove results.
  std::vector<std::string> ids{"A", "B", "C"};
  auto results = make_results(ids);
  for (const auto& result : results)
    ranker.Remove(result.get());
  Wait();

  // Check proto for records of removed results.
  RemovedResultsProto proto = ReadFromDisk();
  EXPECT_EQ(proto.removed_ids_size(), 3);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(ids, UnorderedElementsAreArray(recorded_ids));
}

TEST_F(RemovedResultsRankerTest, DuplicateRemoveRequests) {
  RemovedResultsRanker ranker(GetProto());
  Wait();

  // Request to remove results, with a duplicate.
  std::vector<std::string> ids{"A", "B", "B"};
  auto results = make_results(ids);
  for (const auto& result : results)
    ranker.Remove(result.get());
  Wait();

  // Check proto for records of removed results.
  RemovedResultsProto proto = ReadFromDisk();
  EXPECT_EQ(proto.removed_ids_size(), 2);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(recorded_ids, UnorderedElementsAre("A", "B"));
}

TEST_F(RemovedResultsRankerTest, UpdateResultRanks) {
  RemovedResultsRanker ranker(GetProto());
  Wait();

  // Request to remove some results.
  ranker.Remove(make_result("A").get());
  ranker.Remove(make_result("C").get());
  ranker.Remove(make_result("E").get());
  Wait();

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] = make_results({"A", "B"});
  results_map[ResultType::kInternalApp] = make_results({"C", "D"});
  results_map[ResultType::kOmnibox] = make_results({"E"});

  // Installed apps: The 0th result ("A") is marked to be filtered.
  ranker.UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filter);
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][1]->scoring().filter);

  // Internal apps: The 0th result ("C") is marked to be filtered.
  ranker.UpdateResultRanks(results_map, ResultType::kInternalApp);
  EXPECT_TRUE(results_map[ResultType::kInternalApp][0]->scoring().filter);
  EXPECT_FALSE(results_map[ResultType::kInternalApp][1]->scoring().filter);

  // Omnibox: The 0th result ("C") is marked to be filtered.
  //
  // TODO(crbug.com/1272361): Ranking here should not affect Omnibox results,
  // after support is added to the autocomplete controller for removal of
  // non-zero state Omnibox results.
  ranker.UpdateResultRanks(results_map, ResultType::kOmnibox);
  EXPECT_TRUE(results_map[ResultType::kOmnibox][0]->scoring().filter);

  // Check proto for record of removed results.
  RemovedResultsProto proto = ReadFromDisk();
  EXPECT_EQ(proto.removed_ids_size(), 3);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(recorded_ids, UnorderedElementsAre("A", "C", "E"));
}

TEST_F(RemovedResultsRankerTest, RankEmptyResults) {
  RemovedResultsRanker ranker(GetProto());
  Wait();

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] =
      make_results(std::vector<std::string>());

  ranker.UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp].empty());
}

TEST_F(RemovedResultsRankerTest, RankDuplicateResults) {
  RemovedResultsRanker ranker(GetProto());
  Wait();

  // Request to remove some results.
  ranker.Remove(make_result("A").get());
  ranker.Remove(make_result("C").get());
  Wait();

  ResultsMap results_map;
  // Include some duplicated results.
  results_map[ResultType::kInstalledApp] = make_results({"A", "A", "B"});
  results_map[ResultType::kInternalApp] = make_results({"C", "D"});

  // Installed apps: The 0th and 1st results ("A") are marked to be filtered.
  ranker.UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filter);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][1]->scoring().filter);
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][2]->scoring().filter);

  // Internal apps: The 0th result ("C") is marked to be filtered.
  ranker.UpdateResultRanks(results_map, ResultType::kInternalApp);
  EXPECT_TRUE(results_map[ResultType::kInternalApp][0]->scoring().filter);
  EXPECT_FALSE(results_map[ResultType::kInternalApp][1]->scoring().filter);

  // Check proto for record of removed results.
  RemovedResultsProto proto = ReadFromDisk();
  EXPECT_EQ(proto.removed_ids_size(), 2);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(recorded_ids, UnorderedElementsAre("A", "C"));
}

}  // namespace app_list
