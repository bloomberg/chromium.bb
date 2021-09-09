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

#include "tensorflow/lite/delegates/gpu/cl/kernels/quantize_and_dequantize.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/lite/delegates/gpu/cl/kernels/cl_test.h"
#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/kernels/internal/quantization_util.h"

using ::testing::FloatNear;
using ::testing::Pointwise;

namespace tflite {
namespace gpu {
namespace cl {
namespace {

TEST_F(OpenCLOperationTest, QuantAndDequant_Dim2Bits8) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 3, 2, 1);
  src_tensor.data = {0.0f, 1.0f, 0.25f, 0.50f, 0.4444444f, 0.00001f};

  // Unlike TFLite's FakeQuant kernel, we assume that the incoming values are
  // pre-nudged, since this should be done during model conversion.
  const int num_bits = 8;
  const int quant_min = 0;
  const int quant_max = (1 << num_bits) - 1;
  QuantizeAndDequantizeAttributes attr;
  NudgeQuantizationRange(/**original_min**/ 0.0, /**original_max**/ 1.0,
                         quant_min, quant_max, &attr.min, &attr.max,
                         &attr.scale);

  for (auto storage : env_.GetSupportedStorages()) {
    for (auto precision : env_.GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-2f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      QuantizeAndDequantize operation;
      ASSERT_OK(CreateQuantizeAndDequantize(creation_context_, op_def, attr,
                                            &operation));
      ASSERT_OK(ExecuteGPUOperation(src_tensor, creation_context_, &operation,
                                    BHWC(1, 3, 2, 1), &dst_tensor));
      EXPECT_THAT(dst_tensor.data,
                  Pointwise(FloatNear(eps), {0.0f, 1.0f, 0.25098f, 0.498039f,
                                             0.443137f, 0.0f}));
    }
  }
}

TEST_F(OpenCLOperationTest, QuantAndDequant_Dim3Bits8_NegativeRange) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 3, 1, 2);
  src_tensor.data = {0.0f, -0.9f, 0.25f, 0.50f, 0.4444444f, -0.00001f};

  // Unlike TFLite's FakeQuant kernel, we assume that the incoming values are
  // pre-nudged, since this should be done during model conversion.
  const int num_bits = 8;
  const int quant_min = 0;
  const int quant_max = (1 << num_bits) - 1;
  QuantizeAndDequantizeAttributes attr;
  NudgeQuantizationRange(/**original_min**/ -0.9, /**original_max**/ 0.9,
                         quant_min, quant_max, &attr.min, &attr.max,
                         &attr.scale);

  for (auto storage : env_.GetSupportedStorages()) {
    for (auto precision : env_.GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-2f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      QuantizeAndDequantize operation;
      ASSERT_OK(CreateQuantizeAndDequantize(creation_context_, op_def, attr,
                                            &operation));
      ASSERT_OK(ExecuteGPUOperation(src_tensor, creation_context_, &operation,
                                    BHWC(1, 3, 1, 2), &dst_tensor));
      EXPECT_THAT(dst_tensor.data,
                  Pointwise(FloatNear(eps), {0.0f, -0.896471f, 0.247059f,
                                             0.501176f, 0.444706f, 0.0f}));
    }
  }
}

TEST_F(OpenCLOperationTest, QuantAndDequant_Dim3Bits16) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 3, 1, 2);
  src_tensor.data = {0.0f, 1.0f, 0.25f, 0.50f, 0.4444444f, 0.00001f};

  // Unlike TFLite's FakeQuant kernel, we assume that the incoming values are
  // pre-nudged, since this should be done during model conversion.
  const int num_bits = 16;
  const int quant_min = 0;
  const int quant_max = (1 << num_bits) - 1;
  QuantizeAndDequantizeAttributes attr;
  NudgeQuantizationRange(/**original_min**/ 0.0, /**original_max**/ 1.0,
                         quant_min, quant_max, &attr.min, &attr.max,
                         &attr.scale);

  for (auto storage : env_.GetSupportedStorages()) {
    for (auto precision : env_.GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-3f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      QuantizeAndDequantize operation;
      ASSERT_OK(CreateQuantizeAndDequantize(creation_context_, op_def, attr,
                                            &operation));
      ASSERT_OK(ExecuteGPUOperation(src_tensor, creation_context_, &operation,
                                    BHWC(1, 3, 1, 2), &dst_tensor));
      EXPECT_THAT(dst_tensor.data,
                  Pointwise(FloatNear(eps), {0.0f, 1.0f, 0.250004f, 0.500008f,
                                             0.44445f, 1.5259e-05f}));
    }
  }
}

TEST_F(OpenCLOperationTest, QuantAndDequant_Dim2Bits16_NegativeRange) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 3, 2, 1);
  src_tensor.data = {0.0f, -0.9f, 0.25f, 0.50f, 0.4444444f, -0.00001f};

  // Unlike TFLite's FakeQuant kernel, we assume that the incoming values are
  // pre-nudged, since this should be done during model conversion.
  const int num_bits = 16;
  const int quant_min = 0;
  const int quant_max = (1 << num_bits) - 1;
  QuantizeAndDequantizeAttributes attr;
  NudgeQuantizationRange(/**original_min**/ -0.9, /**original_max**/ 0.9,
                         quant_min, quant_max, &attr.min, &attr.max,
                         &attr.scale);

  for (auto storage : env_.GetSupportedStorages()) {
    for (auto precision : env_.GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-2f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      QuantizeAndDequantize operation;
      ASSERT_OK(CreateQuantizeAndDequantize(creation_context_, op_def, attr,
                                            &operation));
      ASSERT_OK(ExecuteGPUOperation(src_tensor, creation_context_, &operation,
                                    BHWC(1, 3, 2, 1), &dst_tensor));
      EXPECT_THAT(dst_tensor.data,
                  Pointwise(FloatNear(eps), {0.0f, -0.900014f, 0.249998f,
                                             0.499995f, 0.444431f, 0.0f}));
    }
  }
}

}  // namespace
}  // namespace cl
}  // namespace gpu
}  // namespace tflite
