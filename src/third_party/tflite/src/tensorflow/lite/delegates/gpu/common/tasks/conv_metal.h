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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_METAL_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_METAL_H_

#include <vector>

#include "tensorflow/lite/delegates/gpu/common/gpu_info.h"
#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_operation.h"
#include "tensorflow/lite/delegates/gpu/common/task/weights_layout.h"

namespace tflite {
namespace gpu {

class ConvolutionMetal : public GPUOperation {
 public:
  enum class WeightsUploadType {
    PRIVATE_MEM_SIMD8_BROADCAST,
    PRIVATE_MEM_SIMD16_BROADCAST,
    PRIVATE_MEM_SIMD32_BROADCAST,
    LOCAL_MEM_BY_THREADS,
    GLOBAL_MEM,
    CONSTANT_MEM,
  };

  struct ConvParams {
    int3 block_size;
    int3 work_group_size;
    int3 work_group_launch_order;
    int src_depth_loop_size;
    bool need_src_loop = true;
    bool need_dst_loop = true;
    bool linear_wh;
    bool linear_whs;
    WeightsUploadType weights_upload_type;
    WeightsLayout weights_layout;
    bool different_weights_for_height = false;
    bool x_kernel_is_1;
    bool y_kernel_is_1;
  };

  ConvolutionMetal() = default;
  void GetPossibleKernelWorkGroups(
      TuningType tuning_type, const GpuInfo& gpu_info,
      const KernelInfo& kernel_info,
      std::vector<int3>* work_groups) const override {
    work_groups->push_back(work_group_size_);
  }
  int3 GetGridSize() const override;
  absl::Status BindArguments(ArgumentsBinder* args) override;

  // Move only
  ConvolutionMetal(ConvolutionMetal&& kernel) = default;
  ConvolutionMetal& operator=(ConvolutionMetal&& kernel) = default;
  ConvolutionMetal(const ConvolutionMetal&) = delete;
  ConvolutionMetal& operator=(const ConvolutionMetal&) = delete;

  WeightsDescription GetWeightsDescription() const {
    WeightsDescription desc;
    desc.layout = params_.weights_layout;
    desc.output_group_size = params_.block_size.z;
    return desc;
  }

 private:
  explicit ConvolutionMetal(const OperationDef& definition)
      : GPUOperation(definition) {}
  friend ConvolutionMetal CreateConvolutionMetal(
      const OperationDef& definition, const BHWC& dst_shape,
      const Convolution2DAttributes& attr, const GpuInfo& gpu_info);

  friend ConvolutionMetal CreateConvolutionMetalWino4x4To6x6(
      const OperationDef& definition, const BHWC& dst_shape,
      const Convolution2DAttributes& attr, const GpuInfo& gpu_info);

  int2 padding_;
  int2 dilation_;
  ConvParams params_;
};

ConvolutionMetal CreateConvolutionMetal(const OperationDef& definition,
                                        const BHWC& dst_shape,
                                        const Convolution2DAttributes& attr,
                                        const GpuInfo& gpu_info);

ConvolutionMetal CreateConvolutionMetalWino4x4To6x6(
    const OperationDef& definition, const BHWC& dst_shape,
    const Convolution2DAttributes& attr, const GpuInfo& gpu_info);

bool IsConvolutionMetalSupported(const OperationDef& definition);

}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_METAL_H_
