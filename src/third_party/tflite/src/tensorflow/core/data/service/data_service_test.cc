/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <string>
#include <vector>

#include "tensorflow/core/data/service/common.pb.h"
#include "tensorflow/core/data/service/dispatcher.pb.h"
#include "tensorflow/core/data/service/dispatcher_client.h"
#include "tensorflow/core/data/service/test_cluster.h"
#include "tensorflow/core/data/service/test_util.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/status_matchers.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/protobuf/data_service.pb.h"

namespace tensorflow {
namespace data {
namespace {

using ::tensorflow::data::testing::InterleaveTextlineDataset;
using ::tensorflow::data::testing::RangeDataset;
using ::tensorflow::data::testing::RangeDatasetWithShardHint;
using ::tensorflow::data::testing::WaitWhile;
using ::tensorflow::testing::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Pair;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;

tstring LocalTempFilename() {
  std::string path;
  CHECK(Env::Default()->LocalTempFilename(&path));
  return tstring(path);
}

std::vector<int64_t> Range(const int64 range) {
  std::vector<int64_t> result;
  for (int64 i = 0; i < range; ++i) {
    result.push_back(i);
  }
  return result;
}

TEST(DataServiceTest, RangeDataset_NoShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<int64_t> dataset_client(cluster);

  EXPECT_THAT(
      dataset_client.Read(RangeDataset(20), ProcessingModeDef::OFF,
                          TARGET_WORKERS_AUTO),
      IsOkAndHolds(UnorderedElementsAre(
          Pair(cluster.WorkerAddress(0), ElementsAreArray(Range(20))),
          Pair(cluster.WorkerAddress(1), ElementsAreArray(Range(20))),
          Pair(cluster.WorkerAddress(2), ElementsAreArray(Range(20))),
          Pair(cluster.WorkerAddress(3), ElementsAreArray(Range(20))),
          Pair(cluster.WorkerAddress(4), ElementsAreArray(Range(20))))));
}

TEST(DataServiceTest, RangeDataset_DynamicShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<int64_t> dataset_client(cluster);

  TF_ASSERT_OK_AND_ASSIGN(
      DatasetClient<int64_t>::WorkerResultMap worker_results,
      dataset_client.Read(RangeDataset(20), ProcessingModeDef::DYNAMIC,
                          TARGET_WORKERS_AUTO));

  std::vector<int64_t> result;
  for (const auto& worker_result : worker_results) {
    result.insert(result.end(), worker_result.second.begin(),
                  worker_result.second.end());
  }
  EXPECT_THAT(result, UnorderedElementsAreArray(Range(20)));
}

using DataServiceTest_DataShard =
    ::testing::TestWithParam<ProcessingModeDef::ShardingPolicy>;

TEST_P(DataServiceTest_DataShard, RangeDataset_DataShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<int64_t> dataset_client(cluster);

  EXPECT_THAT(
      dataset_client.Read(RangeDataset(20), GetParam(), TARGET_WORKERS_LOCAL),
      IsOkAndHolds(UnorderedElementsAre(
          Pair(cluster.WorkerAddress(0), ElementsAre(0, 5, 10, 15)),
          Pair(cluster.WorkerAddress(1), ElementsAre(1, 6, 11, 16)),
          Pair(cluster.WorkerAddress(2), ElementsAre(2, 7, 12, 17)),
          Pair(cluster.WorkerAddress(3), ElementsAre(3, 8, 13, 18)),
          Pair(cluster.WorkerAddress(4), ElementsAre(4, 9, 14, 19)))));
}

INSTANTIATE_TEST_SUITE_P(ShardingPolicy, DataServiceTest_DataShard,
                         Values(ProcessingModeDef::FILE_OR_DATA,
                                ProcessingModeDef::DATA));

TEST(DataServiceTest, RangeDataset_HintShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<int64_t> dataset_client(cluster);

  EXPECT_THAT(
      dataset_client.Read(RangeDatasetWithShardHint(20),
                          ProcessingModeDef::HINT, TARGET_WORKERS_LOCAL),
      IsOkAndHolds(UnorderedElementsAre(
          Pair(cluster.WorkerAddress(0), ElementsAre(0, 5, 10, 15)),
          Pair(cluster.WorkerAddress(1), ElementsAre(1, 6, 11, 16)),
          Pair(cluster.WorkerAddress(2), ElementsAre(2, 7, 12, 17)),
          Pair(cluster.WorkerAddress(3), ElementsAre(3, 8, 13, 18)),
          Pair(cluster.WorkerAddress(4), ElementsAre(4, 9, 14, 19)))));
}

TEST(DataServiceTest, TextlineDataset_NoShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<tstring> dataset_client(cluster);
  std::vector<tstring> filenames = {LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename()};

  TF_ASSERT_OK_AND_ASSIGN(
      const DatasetDef dataset,
      InterleaveTextlineDataset(
          filenames, {"0", "1\n1", "2\n2\n2", "3\n3\n3\n3", "4\n4\n4\n4\n4"}));
  std::vector<tstring> expected = {"0", "1", "2", "3", "4", "1", "2", "3",
                                   "4", "2", "3", "4", "3", "4", "4"};
  EXPECT_THAT(
      dataset_client.Read(dataset, ProcessingModeDef::OFF, TARGET_WORKERS_ANY),
      IsOkAndHolds(UnorderedElementsAre(
          Pair(cluster.WorkerAddress(0), ElementsAreArray(expected)),
          Pair(cluster.WorkerAddress(1), ElementsAreArray(expected)),
          Pair(cluster.WorkerAddress(2), ElementsAreArray(expected)),
          Pair(cluster.WorkerAddress(3), ElementsAreArray(expected)),
          Pair(cluster.WorkerAddress(4), ElementsAreArray(expected)))));
}

TEST(DataServiceTest, TextlineDataset_DataShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<tstring> dataset_client(cluster);
  std::vector<tstring> filenames = {LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename()};

  TF_ASSERT_OK_AND_ASSIGN(
      const DatasetDef dataset,
      InterleaveTextlineDataset(
          filenames, {"0", "1\n1", "2\n2\n2", "3\n3\n3\n3", "4\n4\n4\n4\n4"}));
  EXPECT_THAT(dataset_client.Read(dataset, ProcessingModeDef::DATA,
                                  TARGET_WORKERS_LOCAL),
              IsOkAndHolds(UnorderedElementsAre(
                  Pair(cluster.WorkerAddress(0), ElementsAre("0", "1", "3")),
                  Pair(cluster.WorkerAddress(1), ElementsAre("1", "2", "4")),
                  Pair(cluster.WorkerAddress(2), ElementsAre("2", "3", "3")),
                  Pair(cluster.WorkerAddress(3), ElementsAre("3", "4", "4")),
                  Pair(cluster.WorkerAddress(4), ElementsAre("4", "2", "4")))));
}

using DataServiceTest_FileShard =
    ::testing::TestWithParam<ProcessingModeDef::ShardingPolicy>;

TEST_P(DataServiceTest_FileShard, TextlineDataset_FileShard) {
  TestCluster cluster(/*num_workers=*/5);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<tstring> dataset_client(cluster);
  std::vector<tstring> filenames = {LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename(), LocalTempFilename(),
                                    LocalTempFilename()};

  TF_ASSERT_OK_AND_ASSIGN(
      const DatasetDef dataset,
      InterleaveTextlineDataset(
          filenames, {"0", "1\n1", "2\n2\n2", "3\n3\n3\n3", "4\n4\n4\n4\n4"}));
  EXPECT_THAT(
      dataset_client.Read(dataset, ProcessingModeDef::FILE_OR_DATA,
                          TARGET_WORKERS_LOCAL),
      IsOkAndHolds(UnorderedElementsAre(
          Pair(cluster.WorkerAddress(0), ElementsAre("0")),
          Pair(cluster.WorkerAddress(1), ElementsAre("1", "1")),
          Pair(cluster.WorkerAddress(2), ElementsAre("2", "2", "2")),
          Pair(cluster.WorkerAddress(3), ElementsAre("3", "3", "3", "3")),
          Pair(cluster.WorkerAddress(4),
               ElementsAre("4", "4", "4", "4", "4")))));
}

INSTANTIATE_TEST_SUITE_P(ShardingPolicy, DataServiceTest_FileShard,
                         Values(ProcessingModeDef::FILE_OR_DATA,
                                ProcessingModeDef::FILE));

TEST(DataServiceTest, GcMissingClientsWithSmallTimeout) {
  TestCluster::Config config;
  config.num_workers = 5;
  config.job_gc_check_interval_ms = 10;
  config.job_gc_timeout_ms = 10;
  config.client_timeout_ms = 10;
  TestCluster cluster(config);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<tstring> dataset_client(cluster);
  TF_ASSERT_OK_AND_ASSIGN(int64_t job_client_id, dataset_client.CreateJob());
  Env::Default()->SleepForMicroseconds(1000 * 1000);  // 1 second.
  // Job should not be garbage collected before the client has started reading.
  EXPECT_THAT(cluster.NumActiveJobs(), IsOkAndHolds(1));

  TF_ASSERT_OK(dataset_client.GetTasks(job_client_id).status());
  // Job should be garbage collected within 10 seconds.
  absl::Time wait_start = absl::Now();
  TF_ASSERT_OK(WaitWhile([&]() -> StatusOr<bool> {
    TF_ASSIGN_OR_RETURN(size_t num_jobs, cluster.NumActiveJobs());
    return num_jobs > 0;
  }));
  EXPECT_LT(absl::Now(), wait_start + absl::Seconds(10));
}

TEST(DataServiceTest, DontGcMissingClientsWithLargeTimeout) {
  TestCluster::Config config;
  config.num_workers = 5;
  config.job_gc_check_interval_ms = 10;
  config.job_gc_timeout_ms = 10;
  config.client_timeout_ms = 10000000000;
  TestCluster cluster(config);
  TF_ASSERT_OK(cluster.Initialize());
  DatasetClient<tstring> dataset_client(cluster);
  TF_ASSERT_OK(dataset_client.CreateJob().status());
  Env::Default()->SleepForMicroseconds(1000 * 1000);  // 1 second.
  // Job should not be garbage collected, since the client hasn't timed out.
  EXPECT_THAT(cluster.NumActiveJobs(), IsOkAndHolds(1));
}

TEST(DataServiceTest, GetWorkers) {
  TestCluster cluster(1);
  TF_ASSERT_OK(cluster.Initialize());
  DataServiceDispatcherClient dispatcher(cluster.DispatcherAddress(), "grpc");
  std::vector<WorkerInfo> workers;
  TF_EXPECT_OK(dispatcher.GetWorkers(workers));
  EXPECT_EQ(1, workers.size());
}

}  // namespace
}  // namespace data
}  // namespace tensorflow
