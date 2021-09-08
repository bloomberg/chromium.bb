/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.
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
#include "tensorflow/core/kernels/data/experimental/auto_shard_dataset_op.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"
#include "tensorflow/core/kernels/data/shard_dataset_op.h"

namespace tensorflow {
namespace data {
namespace experimental {
namespace {

constexpr char kNodeName[] = "auto_shard_dataset";

class AutoShardDatasetParams : public DatasetParams {
 public:
  template <typename T>
  AutoShardDatasetParams(T input_dataset_params, int64 num_workers, int64 index,
                         int auto_shard_policy, DataTypeVector output_dtypes,
                         std::vector<PartialTensorShape> output_shapes,
                         string node_name)
      : DatasetParams(std::move(output_dtypes), std::move(output_shapes),
                      std::move(node_name)),
        num_workers_(num_workers),
        index_(index),
        auto_shard_policy_(auto_shard_policy) {
    input_dataset_params_.push_back(absl::make_unique<T>(input_dataset_params));
    iterator_prefix_ =
        name_utils::IteratorPrefix(input_dataset_params.dataset_type(),
                                   input_dataset_params.iterator_prefix());
  }

  std::vector<Tensor> GetInputTensors() const override {
    return CreateTensors<int64>(TensorShape({}), {{num_workers_}, {index_}});
  }

  Status GetInputNames(std::vector<string>* input_names) const override {
    input_names->clear();
    input_names->emplace_back(AutoShardDatasetOp::kInputDataset);
    input_names->emplace_back(AutoShardDatasetOp::kNumWorkers);
    input_names->emplace_back(AutoShardDatasetOp::kIndex);
    return Status::OK();
  }

  Status GetAttributes(AttributeVector* attr_vector) const override {
    attr_vector->clear();
    attr_vector->emplace_back(AutoShardDatasetOp::kAutoShardPolicy,
                              auto_shard_policy_);
    attr_vector->emplace_back(AutoShardDatasetOp::kOutputTypes, output_dtypes_);
    attr_vector->emplace_back(AutoShardDatasetOp::kOutputShapes,
                              output_shapes_);
    return Status::OK();
  }

  string dataset_type() const override {
    return AutoShardDatasetOp::kDatasetType;
  }

 private:
  int64 num_workers_;
  int64 index_;
  int auto_shard_policy_;
};

class AutoShardDatasetOpTest : public DatasetOpsTestBase {};

// Test Case 1: simple case.
AutoShardDatasetParams AutoShardDatasetParams1() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/5,
                                /*index=*/2,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// Test Case 2: the index is larger than the available elements.
AutoShardDatasetParams AutoShardDatasetParams2() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 1, 1),
                                /*num_workers=*/5,
                                /*index=*/2,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// Test Case 3: the number of outputs could not be evenly divided by
// num_workers.
AutoShardDatasetParams AutoShardDatasetParams3() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/4,
                                /*index=*/3,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// TODO(feihugis): add more test cases that have ReaderDatasets (e.g. a
// CSVDataset or a TFRecordDataset) in the pipeline.

// Test case 4: the index is greater than the number of workers.
AutoShardDatasetParams AutoShardDatasetParams4() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/5,
                                /*index=*/7,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// Test case 5: the index is negative.
AutoShardDatasetParams AutoShardDatasetParams5() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/5,
                                /*index=*/-3,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// Test case 6: num_workers is negative.
AutoShardDatasetParams AutoShardDatasetParams6() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/-3,
                                /*index=*/1,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

// Test case 7: num_workers is zero.
AutoShardDatasetParams AutoShardDatasetParams7() {
  return AutoShardDatasetParams(RangeDatasetParams(0, 10, 1),
                                /*num_workers=*/0,
                                /*index=*/1,
                                /*auto_shard_policy=*/0,
                                /*output_dtypes=*/{DT_INT64},
                                /*output_shapes=*/{PartialTensorShape({})},
                                /*node_name=*/kNodeName);
}

std::vector<GetNextTestCase<AutoShardDatasetParams>> GetNextTestCases() {
  return {
      {/*dataset_params=*/AutoShardDatasetParams1(),
       /*expected_outputs=*/CreateTensors<int64>(TensorShape{}, {{2}, {7}})},
      {/*dataset_params=*/AutoShardDatasetParams2(),
       /*expected_outputs=*/{}},
      {/*dataset_params=*/AutoShardDatasetParams3(),
       /*expected_outputs=*/CreateTensors<int64>(TensorShape{}, {{3}, {7}})}};
}

ITERATOR_GET_NEXT_TEST_P(AutoShardDatasetOpTest, AutoShardDatasetParams,
                         GetNextTestCases())

TEST_F(AutoShardDatasetOpTest, InvalidArguments) {
  std::vector<AutoShardDatasetParams> invalid_dataset_params = {
      AutoShardDatasetParams4(), AutoShardDatasetParams5(),
      AutoShardDatasetParams6(), AutoShardDatasetParams7()};
  for (const auto& dataset_params : invalid_dataset_params) {
    EXPECT_EQ(Initialize(dataset_params).code(),
              tensorflow::error::INVALID_ARGUMENT);
  }
}

}  // namespace
}  // namespace experimental
}  // namespace data
}  // namespace tensorflow
