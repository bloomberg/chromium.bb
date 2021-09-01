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
#ifndef TENSORFLOW_LITE_DELEGATES_HEXAGON_BUILDERS_TRANSPOSE_CONV_2D_BUILDER_H_
#define TENSORFLOW_LITE_DELEGATES_HEXAGON_BUILDERS_TRANSPOSE_CONV_2D_BUILDER_H_

#include <vector>

#include "tensorflow/lite/delegates/hexagon/builders/op_builder.h"

namespace tflite {
namespace delegates {
namespace hexagon {

class TransposeConv2dOpBuilder : public OpBuilder {
 public:
  explicit TransposeConv2dOpBuilder(GraphBuilder* graph_builder, int op_type)
      : OpBuilder(graph_builder, op_type) {}
  TfLiteStatus PopulateSubGraph(const TfLiteIntArray* inputs,
                                const TfLiteIntArray* outputs,
                                TfLiteContext* context) override;

  TfLiteStatus RegisterOutputs(const TfLiteIntArray* outputs,
                               TfLiteContext* context) override;

  ~TransposeConv2dOpBuilder();

 private:
  // TODO(b/142009955): Combine into common util for all types of Conv.
  TfLiteStatus ProcessPerChannelQuantizedWeights(const TfLiteIntArray* inputs,
                                                 const TfLiteIntArray* outputs,
                                                 TfLiteContext* context,
                                                 float* weights_min,
                                                 float* weights_max);

  TensorID node_output_;
  std::vector<float> transposed_weights_;
  std::vector<int> stride_shape_;
  std::vector<int> weight_shape_, bias_shape_;
  std::vector<int> bias_data_;

  // Non-null only if node has per-channel quantized weights/biases.
  OpBuilder* channel_scales_node_ = nullptr;
  float* scales_data_ = nullptr;
  int num_scale_values_ = 1;
};

}  // namespace hexagon
}  // namespace delegates
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_HEXAGON_BUILDERS_TRANSPOSE_CONV_2D_BUILDER_H_
