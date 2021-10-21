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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_SPECIAL_CONV_POINTWISE_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_SPECIAL_CONV_POINTWISE_H_

#include <vector>

#include "tensorflow/lite/delegates/gpu/common/data_type.h"
#include "tensorflow/lite/delegates/gpu/common/model.h"
#include "tensorflow/lite/delegates/gpu/common/selectors/subgraph.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_operation.h"

namespace tflite {
namespace gpu {

struct ConvPointwiseAttributes {
  std::vector<int2> offsets;
};

GPUOperation CreateConvPointwise(const OperationDef& definition,
                                 const ConvPointwiseAttributes& attr);

absl::Status TryFusedPointwiseConv(
    const GraphFloat32& graph, NodeId first_node_id,
    CalculationsPrecision precision,
    const std::map<ValueId, TensorDescriptor>& tensor_descriptors,
    std::set<NodeId>* consumed_nodes, GPUOperationsSubgraph* gpu_subgraph);

}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_SPECIAL_CONV_POINTWISE_H_
