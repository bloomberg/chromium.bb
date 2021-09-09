/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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
#include <stdint.h>

#include <initializer_list>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/lite/kernels/test_util.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace tflite {
namespace {

using ::testing::ElementsAreArray;

template <typename input_type>
class StridedSliceOpModel : public SingleOpModel {
 public:
  StridedSliceOpModel(std::initializer_list<int> input_shape,
                      std::initializer_list<int> begin_shape,
                      std::initializer_list<int> end_shape,
                      std::initializer_list<int> strides_shape, int begin_mask,
                      int end_mask, int ellipsis_mask, int new_axis_mask,
                      int shrink_axis_mask) {
    input_ = AddInput(GetTensorType<input_type>());
    begin_ = AddInput(TensorType_INT32);
    end_ = AddInput(TensorType_INT32);
    strides_ = AddInput(TensorType_INT32);
    output_ = AddOutput(GetTensorType<input_type>());
    SetBuiltinOp(
        BuiltinOperator_STRIDED_SLICE, BuiltinOptions_StridedSliceOptions,
        CreateStridedSliceOptions(builder_, begin_mask, end_mask, ellipsis_mask,
                                  new_axis_mask, shrink_axis_mask)
            .Union());
    BuildInterpreter({input_shape, begin_shape, end_shape, strides_shape});
  }

  void SetInput(std::initializer_list<input_type> data) {
    PopulateTensor<input_type>(input_, data);
  }
  void SetInput(const std::vector<input_type> data) {
    PopulateTensor<input_type>(input_, data);
  }
  void SetBegin(std::initializer_list<int32_t> data) {
    PopulateTensor<int32_t>(begin_, data);
  }
  void SetEnd(std::initializer_list<int32_t> data) {
    PopulateTensor<int32_t>(end_, data);
  }
  void SetStrides(std::initializer_list<int32_t> data) {
    PopulateTensor<int32_t>(strides_, data);
  }

  std::vector<input_type> GetOutput() {
    return ExtractVector<input_type>(output_);
  }
  std::vector<int> GetOutputShape() { return GetTensorShape(output_); }

 private:
  int input_;
  int begin_;
  int end_;
  int strides_;
  int output_;
};

template <typename T>
class StridedSliceOpTest : public ::testing::Test {};

using DataTypes = ::testing::Types<float, uint8_t, int8_t, int16_t, int32_t>;
TYPED_TEST_SUITE(StridedSliceOpTest, DataTypes);

#ifdef GTEST_HAS_DEATH_TEST
TYPED_TEST(StridedSliceOpTest, UnsupportedInputSize) {
  EXPECT_DEATH(StridedSliceOpModel<TypeParam>({2, 2, 2, 2, 2, 2}, {5}, {5}, {5},
                                              0, 0, 0, 0, 0),
               "StridedSlice op only supports 1D-5D input arrays.");
}

TYPED_TEST(StridedSliceOpTest, UnsupportedArgs) {
  EXPECT_DEATH(
      StridedSliceOpModel<TypeParam>({3, 2}, {2}, {2}, {2}, 0, 0, 1, 0, 0),
      "ellipsis_mask is not implemented yet.");
  EXPECT_DEATH(
      StridedSliceOpModel<TypeParam>({3, 2}, {2}, {2}, {2}, 0, 0, 0, 1, 0),
      "new_axis_mask is not implemented yet.");
}
#endif

TYPED_TEST(StridedSliceOpTest, In1DEmpty) {
  StridedSliceOpModel<TypeParam> m({0}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetBegin({1});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({0}));
}

TYPED_TEST(StridedSliceOpTest, In1D) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_Int32End) {
  StridedSliceOpModel<TypeParam> m({32768}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  std::vector<TypeParam> values;
  for (int i = 0; i < 32768; i++) {
    values.push_back(i);
  }
  m.SetInput(values);
  m.SetBegin({0});
  m.SetEnd({32768});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({32768}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray(values));
}

TYPED_TEST(StridedSliceOpTest, In1D_EmptyOutput) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({10});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({0}));
}

TYPED_TEST(StridedSliceOpTest, In1D_NegativeBegin) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({-3});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_OutOfRangeBegin) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({-5});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_NegativeEnd) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({-2});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2}));
}

TYPED_TEST(StridedSliceOpTest, In1D_OutOfRangeEnd) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({-3});
  m.SetEnd({5});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2, 3, 4}));
}

TYPED_TEST(StridedSliceOpTest, In1D_BeginMask) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 1, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_NegativeBeginNegativeStride) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({-2});
  m.SetEnd({-3});
  m.SetStrides({-1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_OutOfRangeBeginNegativeStride) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({5});
  m.SetEnd({2});
  m.SetStrides({-1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({4}));
}

TYPED_TEST(StridedSliceOpTest, In1D_NegativeEndNegativeStride) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({2});
  m.SetEnd({-4});
  m.SetStrides({-1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({3, 2}));
}

TYPED_TEST(StridedSliceOpTest, In1D_OutOfRangeEndNegativeStride) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({-3});
  m.SetEnd({-5});
  m.SetStrides({-1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2, 1}));
}

TYPED_TEST(StridedSliceOpTest, In1D_EndMask) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 1, 0, 0, 0);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({3});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2, 3, 4}));
}

TYPED_TEST(StridedSliceOpTest, In1D_NegStride) {
  StridedSliceOpModel<TypeParam> m({3}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3});
  m.SetBegin({-1});
  m.SetEnd({-4});
  m.SetStrides({-1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({3, 2, 1}));
}

TYPED_TEST(StridedSliceOpTest, In1D_EvenLenStride2) {
  StridedSliceOpModel<TypeParam> m({2}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2});
  m.SetBegin({0});
  m.SetEnd({2});
  m.SetStrides({2});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1}));
}

TYPED_TEST(StridedSliceOpTest, In1D_OddLenStride2) {
  StridedSliceOpModel<TypeParam> m({3}, {1}, {1}, {1}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3});
  m.SetBegin({0});
  m.SetEnd({3});
  m.SetStrides({2});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 3}));
}

TYPED_TEST(StridedSliceOpTest, In2D_Identity) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({0, 0});
  m.SetEnd({2, 3});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4, 5, 6}));
}

TYPED_TEST(StridedSliceOpTest, In2D) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, 0});
  m.SetEnd({2, 2});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({4, 5}));
}

TYPED_TEST(StridedSliceOpTest, In2D_Stride2) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({0, 0});
  m.SetEnd({2, 3});
  m.SetStrides({2, 2});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 3}));
}

TYPED_TEST(StridedSliceOpTest, In2D_NegStride) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, -1});
  m.SetEnd({2, -4});
  m.SetStrides({2, -1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({6, 5, 4}));
}

TYPED_TEST(StridedSliceOpTest, In2D_BeginMask) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 1, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, 0});
  m.SetEnd({2, 2});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 4, 5}));
}

TYPED_TEST(StridedSliceOpTest, In2D_EndMask) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 2, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, 0});
  m.SetEnd({2, 2});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({4, 5, 6}));
}

TYPED_TEST(StridedSliceOpTest, In2D_NegStrideBeginMask) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 2, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, -2});
  m.SetEnd({2, -4});
  m.SetStrides({1, -1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({6, 5, 4}));
}

TYPED_TEST(StridedSliceOpTest, In2D_NegStrideEndMask) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 2, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({1, -2});
  m.SetEnd({2, -3});
  m.SetStrides({1, -1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({5, 4}));
}

TYPED_TEST(StridedSliceOpTest, In3D_Identity) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({2, 3, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 3, 2}));
  EXPECT_THAT(m.GetOutput(),
              ElementsAreArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));
}

TYPED_TEST(StridedSliceOpTest, In3D_NegStride) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({-1, -1, -1});
  m.SetEnd({-3, -4, -3});
  m.SetStrides({-1, -1, -1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 3, 2}));
  EXPECT_THAT(m.GetOutput(),
              ElementsAreArray({12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}));
}

TYPED_TEST(StridedSliceOpTest, In3D_Strided2) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 0);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({2, 3, 2});
  m.SetStrides({2, 2, 2});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 2, 1}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 5}));
}

TYPED_TEST(StridedSliceOpTest, In1D_ShrinkAxisMask1) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({2});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2}));
}

TYPED_TEST(StridedSliceOpTest, In1D_ShrinkAxisMask1_NegativeSlice) {
  // This is equivalent to tf.range(4)[-1].
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 0, 0, 0, 0, 1);
  m.SetInput({0, 1, 2, 3});
  m.SetBegin({-1});
  m.SetEnd({0});
  m.SetStrides({1});

  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({3}));
}

TYPED_TEST(StridedSliceOpTest, In2D_ShrinkAxis3_NegativeSlice) {
  // This is equivalent to tf.range(4)[:, tf.newaxis][-2, -1].
  StridedSliceOpModel<TypeParam> m({4, 1}, {2}, {2}, {2}, 0, 0, 0, 0, 3);
  m.SetInput({0, 1, 2, 3});
  m.SetBegin({-2, -1});
  m.SetEnd({-1, 0});
  m.SetStrides({1, 1});

  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({2}));
}

TYPED_TEST(StridedSliceOpTest, In2D_ShrinkAxis2_BeginEndAxis1_NegativeSlice) {
  // This is equivalent to tf.range(4)[:, tf.newaxis][:, -1].
  StridedSliceOpModel<TypeParam> m({4, 1}, {2}, {2}, {2}, 1, 1, 0, 0, 2);
  m.SetInput({0, 1, 2, 3});
  m.SetBegin({0, -1});
  m.SetEnd({0, 0});
  m.SetStrides({1, 1});

  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({4}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({0, 1, 2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In1D_BeginMaskShrinkAxisMask1) {
  StridedSliceOpModel<TypeParam> m({4}, {1}, {1}, {1}, 1, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4});
  m.SetBegin({1});
  m.SetEnd({1});
  m.SetStrides({1});
  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1}));
}

TYPED_TEST(StridedSliceOpTest, In2D_ShrinkAxisMask1) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({0, 0});
  m.SetEnd({1, 3});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3}));
}

TYPED_TEST(StridedSliceOpTest, In2D_ShrinkAxisMask2) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 2);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({0, 0});
  m.SetEnd({2, 1});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 4}));
}

TYPED_TEST(StridedSliceOpTest, In2D_ShrinkAxisMask3) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 0, 0, 0, 0, 3);
  m.SetInput({1, 2, 3, 4, 5, 6});
  m.SetBegin({0, 0});
  m.SetEnd({1, 1});
  m.SetStrides({1, 1});
  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis1) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 3, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4, 5, 6}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis2) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 2);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({2, 1, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 7, 8}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis3) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 3);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 1, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis4) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 4);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({2, 3, 1});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 3, 5, 7, 9, 11}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis5) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 5);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 3, 1});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 3, 5}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis6) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 6);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({2, 1, 1});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 7}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis7) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 7);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 1, 1});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_TRUE(m.GetOutputShape().empty());
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1}));
}

// This tests catches a very subtle bug that was fixed by cl/188403234.
TYPED_TEST(StridedSliceOpTest, RunTwice) {
  StridedSliceOpModel<TypeParam> m({2, 3}, {2}, {2}, {2}, 1, 0, 0, 0, 0);

  auto setup_inputs = [&m]() {
    m.SetInput({1, 2, 3, 4, 5, 6});
    m.SetBegin({1, 0});
    m.SetEnd({2, 2});
    m.SetStrides({1, 1});
  };

  setup_inputs();
  m.Invoke();
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 4, 5}));

  setup_inputs();
  m.Invoke();
  // Prior to cl/188403234 this was {4, 5}.
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 4, 5}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis1Uint8) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 3, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4, 5, 6}));
}

TYPED_TEST(StridedSliceOpTest, In3D_IdentityShrinkAxis1int8) {
  StridedSliceOpModel<TypeParam> m({2, 3, 2}, {3}, {3}, {3}, 0, 0, 0, 0, 1);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  m.SetBegin({0, 0, 0});
  m.SetEnd({1, 3, 2});
  m.SetStrides({1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4, 5, 6}));
}

TYPED_TEST(StridedSliceOpTest, In5D_Identity) {
  StridedSliceOpModel<TypeParam> m({2, 2, 2, 1, 2}, {5}, {5}, {5}, 0, 0, 0, 0,
                                   0);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  m.SetBegin({0, 0, 0, 0, 0});
  m.SetEnd({2, 1, 2, 1, 2});
  m.SetStrides({1, 1, 1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({2, 1, 2, 1, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4, 9, 10, 11, 12}));
}

TYPED_TEST(StridedSliceOpTest, In5D_IdentityShrinkAxis1) {
  StridedSliceOpModel<TypeParam> m({2, 2, 2, 1, 2}, {5}, {5}, {5}, 0, 0, 0, 0,
                                   1);
  m.SetInput({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  m.SetBegin({0, 0, 0, 0, 0});
  m.SetEnd({2, 1, 2, 1, 2});
  m.SetStrides({1, 1, 1, 1, 1});
  m.Invoke();
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1, 2, 1, 2}));
  EXPECT_THAT(m.GetOutput(), ElementsAreArray({1, 2, 3, 4}));
}
}  // namespace
}  // namespace tflite
