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
#include "tensorflow/core/data/service/dispatcher_client.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "tensorflow/core/data/service/data_transfer.h"
#include "tensorflow/core/data/service/test_cluster.h"
#include "tensorflow/core/data/service/test_util.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status_matchers.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/protobuf/data_service.pb.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"

namespace tensorflow {
namespace data {
namespace {

using ::tensorflow::data::testing::EqualsProto;
using ::tensorflow::testing::StatusIs;
using ::testing::HasSubstr;

constexpr const char kProtocol[] = "grpc";

class DispatcherClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_cluster_ = absl::make_unique<TestCluster>(/*num_workers=*/1);
    TF_ASSERT_OK(test_cluster_->Initialize());
    dispatcher_client_ = absl::make_unique<DataServiceDispatcherClient>(
        test_cluster_->DispatcherAddress(), kProtocol);
  }

  // Creates a dataset and returns the dataset ID.
  StatusOr<int64_t> RegisterDataset(const DataServiceMetadata& metadata) {
    const auto dataset_def = testing::RangeDataset(10);
    int64_t dataset_id = 0;
    TF_RETURN_IF_ERROR(
        dispatcher_client_->RegisterDataset(dataset_def, metadata, dataset_id));
    return dataset_id;
  }

  std::unique_ptr<TestCluster> test_cluster_;
  std::unique_ptr<DataServiceDispatcherClient> dispatcher_client_;
};

TEST_F(DispatcherClientTest, GetDataServiceMetadata) {
  DataServiceMetadata metadata;
  metadata.set_element_spec("encoded_element_spec");
  metadata.set_compression(DataServiceMetadata::COMPRESSION_SNAPPY);
  metadata.set_cardinality(kInfiniteCardinality);
  TF_ASSERT_OK_AND_ASSIGN(const int64_t dataset_id, RegisterDataset(metadata));

  DataServiceMetadata result;
  TF_ASSERT_OK(dispatcher_client_->GetDataServiceMetadata(dataset_id, result));
  EXPECT_THAT(result, EqualsProto(metadata));
}

TEST_F(DispatcherClientTest, DatasetDoesNotExist) {
  DataServiceMetadata metadata;
  EXPECT_THAT(
      dispatcher_client_->GetDataServiceMetadata(
          /*dataset_id=*/-1000, metadata),
      StatusIs(error::NOT_FOUND, HasSubstr("Dataset id -1000 not found")));
}

TEST_F(DispatcherClientTest, GetDataServiceConfig) {
  DataServiceConfig config;
  TF_ASSERT_OK(dispatcher_client_->GetDataServiceConfig(config));
  EXPECT_EQ(config.deployment_mode(), DEPLOYMENT_MODE_COLOCATED);
}

}  // namespace
}  // namespace data
}  // namespace tensorflow
