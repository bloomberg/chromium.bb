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

#include "tensorflow/lite/delegates/gpu/common/tasks/winograd_test_util.h"

#include <vector>

#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/task/testing_util.h"
#include "tensorflow/lite/delegates/gpu/common/tasks/winograd.h"
#include "tensorflow/lite/delegates/gpu/common/winograd_util.h"

namespace tflite {
namespace gpu {

absl::Status Winograd4x4To36TileX6Test(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 4, 4, 1);
  src_tensor.data.resize(16);
  for (int i = 0; i < 16; ++i) {
    src_tensor.data[i] = sin(i);
  }

  TensorFloat32 dst_ref;
  dst_ref.shape = BHWC(1, 36, 1, 1);
  dst_ref.data.resize(36, 0.0f);
  auto b_t = BtMatrixForWinograd4x4To6x6();

  // Bt * Src * B
  // 1: temp = Src * B
  std::vector<float> temp(36, 0.0f);
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 6; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        if (y < 1 || y > 4 || i < 1 || i > 4) continue;
        const int index = src_tensor.shape.LinearIndex({0, y - 1, i - 1, 0});
        sum += src_tensor.data[index] * b_t[x * 6 + i];
      }
      temp[y * 6 + x] = sum;
    }
  }
  // 2: ref = Bt * temp
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 6; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        sum += b_t[y * 6 + i] * temp[i * 6 + x];
      }
      const int index = dst_ref.shape.LinearIndex({0, y * 6 + x, 0, 0});
      dst_ref.data[index] = sum;
    }
  }

  for (auto storage : env->GetSupportedStorages()) {
    for (auto precision : env->GetSupportedPrecisions()) {
      float eps = precision == CalculationsPrecision::F32 ? 1e-5f : 1e-2f;
      if (!env->GetGpuInfo().IsRoundToNearestSupported()) {
        eps *= 4.0f;
      }
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      Padding2D padding;
      padding.prepended = HW(1, 1);
      padding.appended = HW(1, 1);
      Winograd4x4To36TileX6 operation =
          CreateWinograd4x4To36TileX6(env->GetGpuInfo(), op_def, padding);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor,
          absl::make_unique<Winograd4x4To36TileX6>(std::move(operation)),
          BHWC(1, 36, 1, 1), &dst_tensor));
      RETURN_IF_ERROR(PointWiseNear(dst_ref.data, dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

absl::Status Winograd36To4x4Tile4x1Test(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 36, 1, 1);
  src_tensor.data.resize(36);
  for (int i = 0; i < 36; ++i) {
    src_tensor.data[i] = sin(i);
  }

  ::tflite::gpu::Tensor<Linear, DataType::FLOAT32> biases;
  biases.shape = Linear(1);
  biases.data.resize(biases.shape.DimensionsProduct());
  for (int i = 0; i < biases.data.size(); ++i) {
    biases.data[i] = 0.0f;
  }

  TensorFloat32 dst_ref;
  dst_ref.shape = BHWC(1, 4, 4, 1);
  dst_ref.data.resize(16, 0.0f);
  auto a_t = AtMatrixForWinograd4x4To6x6();

  // At * Src * A
  // 1: temp = Src * A
  std::vector<float> temp(24, 0.0f);
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 4; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        const int index = src_tensor.shape.LinearIndex({0, y * 6 + i, 0, 0});
        sum += src_tensor.data[index] * a_t[x * 6 + i];
      }
      temp[y * 4 + x] = sum;
    }
  }
  // 2: ref = At * temp
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        sum += a_t[y * 6 + i] * temp[i * 4 + x];
      }
      const int index = dst_ref.shape.LinearIndex({0, y, x, 0});
      dst_ref.data[index] = sum;
    }
  }

  for (auto storage : env->GetSupportedStorages()) {
    for (auto precision : env->GetSupportedPrecisions()) {
      float eps = precision == CalculationsPrecision::F32 ? 1e-5f : 1e-2f;
      if (!env->GetGpuInfo().IsRoundToNearestSupported()) {
        eps *= 4.0f;
      }
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      Winograd36To4x4Tile4x1 operation =
          CreateWinograd36To4x4Tile4x1(env->GetGpuInfo(), op_def, biases);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor,
          absl::make_unique<Winograd36To4x4Tile4x1>(std::move(operation)),
          BHWC(1, 4, 4, 1), &dst_tensor));
      RETURN_IF_ERROR(PointWiseNear(dst_ref.data, dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

absl::Status Winograd4x4To36Test(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 4, 4, 1);
  src_tensor.data.resize(16);
  for (int i = 0; i < 16; ++i) {
    src_tensor.data[i] = sin(i);
  }

  TensorFloat32 dst_ref;
  dst_ref.shape = BHWC(1, 36, 1, 1);
  dst_ref.data.resize(36, 0.0f);
  auto b_t = BtMatrixForWinograd4x4To6x6();

  // Bt * Src * B
  // 1: temp = Src * B
  std::vector<float> temp(36, 0.0f);
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 6; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        if (y < 1 || y > 4 || i < 1 || i > 4) continue;
        const int index = src_tensor.shape.LinearIndex({0, y - 1, i - 1, 0});
        sum += src_tensor.data[index] * b_t[x * 6 + i];
      }
      temp[y * 6 + x] = sum;
    }
  }
  // 2: ref = Bt * temp
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 6; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        sum += b_t[y * 6 + i] * temp[i * 6 + x];
      }
      const int index = dst_ref.shape.LinearIndex({0, y * 6 + x, 0, 0});
      dst_ref.data[index] = sum;
    }
  }

  for (auto storage :
       {TensorStorageType::BUFFER, TensorStorageType::IMAGE_BUFFER}) {
    for (auto precision : env->GetSupportedPrecisions()) {
      float eps = precision == CalculationsPrecision::F32 ? 1e-5f : 1e-2f;
      if (!env->GetGpuInfo().IsRoundToNearestSupported()) {
        eps *= 4.0f;
      }
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      Padding2D padding;
      padding.prepended = HW(1, 1);
      padding.appended = HW(1, 1);
      Winograd4x4To36 operation = CreateWinograd4x4To36(op_def, padding);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor, absl::make_unique<Winograd4x4To36>(std::move(operation)),
          BHWC(1, 36, 1, 1), &dst_tensor));
      RETURN_IF_ERROR(PointWiseNear(dst_ref.data, dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

absl::Status Winograd36To4x4Test(TestExecutionEnvironment* env) {
  TensorFloat32 src_tensor;
  src_tensor.shape = BHWC(1, 36, 1, 1);
  src_tensor.data.resize(36);
  for (int i = 0; i < 36; ++i) {
    src_tensor.data[i] = sin(i);
  }

  ::tflite::gpu::Tensor<Linear, DataType::FLOAT32> biases;
  biases.shape = Linear(1);
  biases.data.resize(biases.shape.DimensionsProduct());
  for (int i = 0; i < biases.data.size(); ++i) {
    biases.data[i] = 0.0f;
  }

  TensorFloat32 dst_ref;
  dst_ref.shape = BHWC(1, 4, 4, 1);
  dst_ref.data.resize(16, 0.0f);
  auto a_t = AtMatrixForWinograd4x4To6x6();

  // At * Src * A
  // 1: temp = Src * A
  std::vector<float> temp(24, 0.0f);
  for (int y = 0; y < 6; ++y) {
    for (int x = 0; x < 4; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        const int index = src_tensor.shape.LinearIndex({0, y * 6 + i, 0, 0});
        sum += src_tensor.data[index] * a_t[x * 6 + i];
      }
      temp[y * 4 + x] = sum;
    }
  }
  // 2: ref = At * temp
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 6; ++i) {
        sum += a_t[y * 6 + i] * temp[i * 4 + x];
      }
      const int index = dst_ref.shape.LinearIndex({0, y, x, 0});
      dst_ref.data[index] = sum;
    }
  }

  for (auto storage :
       {TensorStorageType::BUFFER, TensorStorageType::IMAGE_BUFFER}) {
    for (auto precision : env->GetSupportedPrecisions()) {
      float eps = precision == CalculationsPrecision::F32 ? 1e-5f : 1e-2f;
      if (!env->GetGpuInfo().IsRoundToNearestSupported()) {
        eps *= 4.0f;
      }
      OperationDef op_def;
      op_def.precision = precision;
      auto data_type = DeduceDataTypeFromPrecision(precision);
      op_def.src_tensors.push_back({data_type, storage, Layout::HWC});
      op_def.dst_tensors.push_back({data_type, storage, Layout::HWC});
      TensorFloat32 dst_tensor;
      Winograd36To4x4 operation = CreateWinograd36To4x4(op_def, biases);
      RETURN_IF_ERROR(env->ExecuteGPUOperation(
          src_tensor, absl::make_unique<Winograd36To4x4>(std::move(operation)),
          BHWC(1, 4, 4, 1), &dst_tensor));
      RETURN_IF_ERROR(PointWiseNear(dst_ref.data, dst_tensor.data, eps));
    }
  }
  return absl::OkStatus();
}

}  // namespace gpu
}  // namespace tflite
