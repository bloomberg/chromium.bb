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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_WEIGHTS_CONVERTER_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_WEIGHTS_CONVERTER_H_

#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_operation.h"
#include "tensorflow/lite/delegates/gpu/common/task/weights_layout.h"
#include "tensorflow/lite/delegates/gpu/common/types.h"

namespace tflite {
namespace gpu {

class ConverterToConvWeights : public GPUOperation {
 public:
  ConverterToConvWeights(const OperationDef& definition,
                         const WeightsDescription& weights_desc);
  absl::Status BindArguments(ArgumentsBinder* args) override;
  int3 GetGridSize() const override;

  // Move only
  ConverterToConvWeights(ConverterToConvWeights&& operation);
  ConverterToConvWeights& operator=(ConverterToConvWeights&& operation);
  ConverterToConvWeights(const ConverterToConvWeights&) = delete;
  ConverterToConvWeights& operator=(const ConverterToConvWeights&) = delete;

 private:
  std::string GetConverterToConvWeightsCode(
      const OperationDef& op_def, const WeightsDescription& weights_desc);

  WeightsDescription weights_desc_;
};

// We expect src BHWC tensor and we assume that B is O, H = H, W = W, C is I
// as dst we expect Tensor with storage type BUFFER and
// dst.b * dst.h * dst.w * dst.c = AlignByN(src.b, 4) * src.h * src.w
// AlignByN(src.c, 4)
ConverterToConvWeights CreateConverterToConvWeights(
    const OperationDef& definition, const WeightsDescription& weights_desc);

}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_COMMON_TASKS_CONV_WEIGHTS_CONVERTER_H_
