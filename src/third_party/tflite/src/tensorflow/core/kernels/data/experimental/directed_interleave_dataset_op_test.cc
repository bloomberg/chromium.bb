/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
#include "tensorflow/core/kernels/data/experimental/directed_interleave_dataset_op.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"

namespace tensorflow {
namespace data {
namespace experimental {
namespace {

constexpr char kNodeName[] = "directed_interleave_dataset";

class DirectedInterleaveDatasetParams : public DatasetParams {
 public:
  template <typename S, typename T>
  DirectedInterleaveDatasetParams(S selector_input_dataset_params,
                                  std::vector<T> input_dataset_params_vec,
                                  DataTypeVector output_dtypes,
                                  std::vector<PartialTensorShape> output_shapes,
                                  int num_input_datasets, string node_name)
      : DatasetParams(std::move(output_dtypes), std::move(output_shapes),
                      std::move(node_name)),
        num_input_datasets_(num_input_datasets) {
    input_dataset_params_.push_back(
        absl::make_unique<S>(selector_input_dataset_params));
    for (auto input_dataset_params : input_dataset_params_vec) {
      input_dataset_params_.push_back(
          absl::make_unique<T>(input_dataset_params));
    }

    if (!input_dataset_params_vec.empty()) {
      iterator_prefix_ = name_utils::IteratorPrefix(
          input_dataset_params_vec[0].dataset_type(),
          input_dataset_params_vec[0].iterator_prefix());
    }
  }

  std::vector<Tensor> GetInputTensors() const override { return {}; }

  Status GetInputNames(std::vector<string>* input_names) const override {
    input_names->clear();
    input_names->emplace_back(
        DirectedInterleaveDatasetOp::kSelectorInputDataset);
    for (int i = 0; i < num_input_datasets_; ++i) {
      input_names->emplace_back(absl::StrCat(
          DirectedInterleaveDatasetOp::kDataInputDatasets, "_", i));
    }
    return Status::OK();
  }

  Status GetAttributes(AttributeVector* attr_vector) const override {
    attr_vector->clear();
    attr_vector->emplace_back(DirectedInterleaveDatasetOp::kOutputTypes,
                              output_dtypes_);
    attr_vector->emplace_back(DirectedInterleaveDatasetOp::kOutputShapes,
                              output_shapes_);
    attr_vector->emplace_back(DirectedInterleaveDatasetOp::kNumInputDatasets,
                              num_input_datasets_);
    return Status::OK();
  }

  string dataset_type() const override {
    return DirectedInterleaveDatasetOp::kDatasetType;
  }

 private:
  int32 num_input_datasets_;
};

class DirectedInterleaveDatasetOpTest : public DatasetOpsTestBase {};

DirectedInterleaveDatasetParams AlternateInputsParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams SelectExhaustedInputParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 2, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams OneInputDatasetParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 0, 0, 0, 0, 0})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 6, 1)},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*num_input_datasets=*/1,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams ZeroInputDatasetParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 0, 0, 0, 0, 0})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/std::vector<RangeDatasetParams>{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*num_input_datasets=*/0,
      /*node_name=*/kNodeName);
}

// Test case: `num_input_datasets` is larger than the size of
// `input_dataset_params_vec`.
DirectedInterleaveDatasetParams LargeNumInputDatasetsParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/5,
      /*node_name=*/kNodeName);
}

// Test case: `num_input_datasets` is smaller than the size of
// `input_dataset_params_vec`.
DirectedInterleaveDatasetParams SmallNumInputDatasetsParams() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/1,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams InvalidSelectorOuputDataType() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int32>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams InvalidSelectorOuputShape() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6, 1},
                                          {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams InvalidSelectorValues() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {2, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{RangeDatasetParams(0, 3, 1),
                                      RangeDatasetParams(10, 13, 1)},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

DirectedInterleaveDatasetParams InvalidInputDatasetsDataType() {
  auto selector_input_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{6}, {0, 1, 0, 1, 0, 1})},
      /*node_name=*/"tensor_slice");
  return DirectedInterleaveDatasetParams(
      selector_input_dataset_params,
      /*input_dataset_params_vec=*/
      std::vector<RangeDatasetParams>{
          RangeDatasetParams(0, 3, 1, {DT_INT32}),
          RangeDatasetParams(10, 13, 1, {DT_INT64})},
      /*output_dtypes=*/{DT_INT64, DT_INT64},
      /*output_shapes=*/{PartialTensorShape({}), PartialTensorShape({})},
      /*num_input_datasets=*/2,
      /*node_name=*/kNodeName);
}

std::vector<GetNextTestCase<DirectedInterleaveDatasetParams>>
GetNextTestCases() {
  return {{/*dataset_params=*/AlternateInputsParams(),
           /*expected_outputs=*/{CreateTensors<int64>(
               TensorShape({}), {{0}, {10}, {1}, {11}, {2}, {12}})}},
          {/*dataset_params=*/SelectExhaustedInputParams(),
           /*expected_outputs=*/{CreateTensors<int64>(
               TensorShape({}), {{0}, {10}, {1}, {11}, {12}})}},
          {/*dataset_params=*/OneInputDatasetParams(),
           /*expected_outputs=*/{CreateTensors<int64>(
               TensorShape({}), {{0}, {1}, {2}, {3}, {4}, {5}})}},
          {/*dataset_params=*/LargeNumInputDatasetsParams(),
           /*expected_outputs=*/{CreateTensors<int64>(
               TensorShape({}), {{0}, {10}, {1}, {11}, {2}, {12}})}},
          {/*dataset_params=*/SmallNumInputDatasetsParams(),
           /*expected_outputs=*/{CreateTensors<int64>(
               TensorShape({}), {{0}, {10}, {1}, {11}, {2}, {12}})}}};
}

ITERATOR_GET_NEXT_TEST_P(DirectedInterleaveDatasetOpTest,
                         DirectedInterleaveDatasetParams, GetNextTestCases())

TEST_F(DirectedInterleaveDatasetOpTest, DatasetNodeName) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetNodeName(dataset_params.node_name()));
}

TEST_F(DirectedInterleaveDatasetOpTest, DatasetTypeString) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetTypeString(
      name_utils::OpName(DirectedInterleaveDatasetOp::kDatasetType)));
}

TEST_F(DirectedInterleaveDatasetOpTest, DatasetOutputDtypes) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputDtypes({DT_INT64}));
}

TEST_F(DirectedInterleaveDatasetOpTest, DatasetOutputShapes) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputShapes({PartialTensorShape({})}));
}

TEST_F(DirectedInterleaveDatasetOpTest, Cardinality) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetCardinality(kUnknownCardinality));
}

TEST_F(DirectedInterleaveDatasetOpTest, IteratorOutputDtypes) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputDtypes({DT_INT64}));
}

TEST_F(DirectedInterleaveDatasetOpTest, IteratorOutputShapes) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputShapes({PartialTensorShape({})}));
}

TEST_F(DirectedInterleaveDatasetOpTest, IteratorPrefix) {
  auto dataset_params = AlternateInputsParams();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorPrefix(
      name_utils::IteratorPrefix(DirectedInterleaveDatasetOp::kDatasetType,
                                 dataset_params.iterator_prefix())));
}

std::vector<IteratorSaveAndRestoreTestCase<DirectedInterleaveDatasetParams>>
IteratorSaveAndRestoreTestCases() {
  return {
      {/*dataset_params=*/AlternateInputsParams(),
       /*breakpoints=*/{0, 5, 8},
       /*expected_outputs=*/
       CreateTensors<int64>(TensorShape{}, {{0}, {10}, {1}, {11}, {2}, {12}}),
       /*compare_order=*/true},
      {/*dataset_params=*/SelectExhaustedInputParams(),
       /*breakpoints=*/{0, 4, 8},
       /*expected_outputs=*/
       CreateTensors<int64>(TensorShape{}, {{0}, {10}, {1}, {11}, {12}}),
       /*compare_order=*/true},
      {/*dataset_params=*/OneInputDatasetParams(),
       /*breakpoints=*/{0, 5, 8},
       /*expected_outputs=*/
       {CreateTensors<int64>(TensorShape({}), {{0}, {1}, {2}, {3}, {4}, {5}})}},
      {/*dataset_params=*/LargeNumInputDatasetsParams(),
       /*breakpoints=*/{0, 5, 8},
       /*expected_outputs=*/
       {CreateTensors<int64>(TensorShape({}),
                             {{0}, {10}, {1}, {11}, {2}, {12}})}},
      {/*dataset_params=*/SmallNumInputDatasetsParams(),
       /*breakpoints=*/{0, 5, 8},
       /*expected_outputs=*/
       {CreateTensors<int64>(TensorShape({}),
                             {{0}, {10}, {1}, {11}, {2}, {12}})}}};
}

ITERATOR_SAVE_AND_RESTORE_TEST_P(DirectedInterleaveDatasetOpTest,
                                 DirectedInterleaveDatasetParams,
                                 IteratorSaveAndRestoreTestCases())

TEST_F(DirectedInterleaveDatasetOpTest, InvalidArguments) {
  std::vector<DirectedInterleaveDatasetParams> invalid_params_vec = {
      InvalidSelectorOuputDataType(), InvalidSelectorOuputShape(),
      InvalidInputDatasetsDataType(), ZeroInputDatasetParams()};
  for (auto& dataset_params : invalid_params_vec) {
    EXPECT_EQ(Initialize(dataset_params).code(),
              tensorflow::error::INVALID_ARGUMENT);
  }
}

TEST_F(DirectedInterleaveDatasetOpTest, InvalidSelectorValues) {
  auto dataset_params = InvalidSelectorValues();
  TF_ASSERT_OK(Initialize(dataset_params));
  bool end_of_sequence = false;
  std::vector<Tensor> next;
  EXPECT_EQ(
      iterator_->GetNext(iterator_ctx_.get(), &next, &end_of_sequence).code(),
      tensorflow::error::INVALID_ARGUMENT);
}

}  // namespace
}  // namespace experimental
}  // namespace data
}  // namespace tensorflow
