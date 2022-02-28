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

#include "tensorflow/lite/delegates/gpu/common/tasks/conv_constants_test_util.h"

#include <vector>

#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/task/testing_util.h"
#include "tensorflow/lite/delegates/gpu/common/tasks/conv_constants.h"

namespace tflite {
namespace gpu {

absl::Status ConvConstantsSimpleWeightsTest(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 2, 2, 2);
  src_tensor.data = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};

  Convolution2DAttributes attr;
  attr.padding.prepended = HW(0, 0);
  attr.padding.appended = HW(1, 1);
  attr.strides = HW(1, 1);
  attr.dilations = HW(1, 1);
  attr.weights.shape = OHWI(1, 2, 2, 2);
  attr.weights.data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  attr.bias.shape = Linear(1);
  attr.bias.data = {0.0f};

  for (auto storage : env->GetSupportedStorages()) {
    for (auto precision : env->GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-3f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      GPUOperation operation =
          CreateConvConstants(env->GetGpuInfo(), op_def, attr);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor, absl::make_unique<GPUOperation>(std::move(operation)),
          BHWC(1, 2, 2, 1), &dst_tensor));
      RETURN_IF_ERROR(
          PointWiseNear({28.0f, 18.0f, 22.0f, 13.0f}, dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

absl::Status ConvConstantsTest(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 2, 2, 2);
  src_tensor.data = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};

  Convolution2DAttributes attr;
  attr.padding.prepended = HW(0, 0);
  attr.padding.appended = HW(1, 1);
  attr.strides = HW(1, 1);
  attr.dilations = HW(1, 1);
  attr.weights.shape = OHWI(2, 2, 2, 2);
  attr.weights.data = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                       9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
  attr.bias.shape = Linear(2);
  attr.bias.data = {0.5f, -0.5f};

  for (auto storage : env->GetSupportedStorages()) {
    for (auto precision : env->GetSupportedPrecisions()) {
      const float eps = precision == CalculationsPrecision::F32 ? 1e-6f : 1e-3f;
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      GPUOperation operation =
          CreateConvConstants(env->GetGpuInfo(), op_def, attr);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor, absl::make_unique<GPUOperation>(std::move(operation)),
          BHWC(1, 2, 2, 2), &dst_tensor));
      RETURN_IF_ERROR(PointWiseNear(
          {168.5f, 391.5f, 80.5f, 223.5f, 60.5f, 235.5f, 20.5f, 123.5f},
          dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

}  // namespace gpu
}  // namespace tflite
