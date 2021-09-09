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

#include "tensorflow/lite/kernels/internal/reference/softmax.h"

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/internal/common.h"
#include "tensorflow/lite/kernels/internal/quantization_util.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/kernels/op_macros.h"

namespace tflite {
namespace ops {
namespace micro {
namespace activations {
namespace {

TfLiteStatus CalculateSoftmaxParams(TfLiteContext* context,
                                    const TfLiteTensor* input,
                                    TfLiteTensor* output,
                                    const TfLiteSoftmaxParams* params,
                                    SoftmaxParams* op_data) {
  if (input->type == kTfLiteUInt8 || input->type == kTfLiteInt8) {
    if (input->type == kTfLiteUInt8) {
      TF_LITE_ENSURE_TYPES_EQ(context, output->type, kTfLiteUInt8);
      TF_LITE_ENSURE_EQ(context, output->params.zero_point, 0);
    } else {
      TF_LITE_ENSURE_TYPES_EQ(context, input->type, kTfLiteInt8);
      if (output->type == kTfLiteInt16) {
        TF_LITE_ENSURE_EQ(context, output->params.zero_point, -32768);
        // NOTE: Current int16 softmax output does not require symmetric scaling
        // - so no need to verify scale here.
      } else {
        TF_LITE_ENSURE_TYPES_EQ(context, output->type, kTfLiteInt8);
        TF_LITE_ENSURE_EQ(context, output->params.zero_point, -128);
        TF_LITE_ENSURE(context, output->params.scale == 1.f / 256);
      }
    }

    static const int kScaledDiffIntegerBits = 5;

    int input_left_shift;
    tflite::PreprocessSoftmaxScaling(
        static_cast<double>(params->beta),
        static_cast<double>(input->params.scale), kScaledDiffIntegerBits,
        &op_data->input_multiplier, &input_left_shift);
    op_data->input_left_shift = input_left_shift;
    op_data->diff_min =
        -1.0 * tflite::CalculateInputRadius(kScaledDiffIntegerBits,
                                            op_data->input_left_shift);
  } else {
    TF_LITE_ENSURE_TYPES_EQ(context, input->type, kTfLiteFloat32);
    TF_LITE_ENSURE_TYPES_EQ(context, output->type, kTfLiteFloat32);
    op_data->beta = static_cast<double>(params->beta);
  }
  return kTfLiteOk;
}

}  // namespace

TfLiteStatus SoftmaxPrepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE_EQ(context, NumInputs(node), 1);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);
  const TfLiteTensor* input = GetInput(context, node, 0);
  TF_LITE_ENSURE(context, NumDimensions(input) >= 1);

  return kTfLiteOk;
}

// Takes a tensor and performs softmax along the last dimension.
void SoftmaxFloat(const TfLiteTensor* input, TfLiteTensor* output,
                  const SoftmaxParams& op_data) {
  tflite::reference_ops::Softmax(
      op_data, GetTensorShape(input), GetTensorData<float>(input),
      GetTensorShape(output), GetTensorData<float>(output));
}

void SoftmaxQuantized(const TfLiteTensor* input, TfLiteTensor* output,
                      const SoftmaxParams& op_data) {
  if (input->type == kTfLiteUInt8) {
    tflite::reference_ops::Softmax(
        op_data, GetTensorShape(input), GetTensorData<uint8_t>(input),
        GetTensorShape(output), GetTensorData<uint8_t>(output));
  } else {
    if (output->type == kTfLiteInt16) {
      tflite::reference_ops::Softmax(
          op_data, GetTensorShape(input), GetTensorData<int8_t>(input),
          GetTensorShape(output), GetTensorData<int16_t>(output));
    } else {
      tflite::reference_ops::Softmax(
          op_data, GetTensorShape(input), GetTensorData<int8_t>(input),
          GetTensorShape(output), GetTensorData<int8_t>(output));
    }
  }
}

TfLiteStatus SoftmaxEval(TfLiteContext* context, TfLiteNode* node) {
  auto* params = static_cast<TfLiteSoftmaxParams*>(node->builtin_data);

  const TfLiteTensor* input = GetInput(context, node, 0);
  TfLiteTensor* output = GetOutput(context, node, 0);

  SoftmaxParams op_data;
  TF_LITE_ENSURE_STATUS(
      CalculateSoftmaxParams(context, input, output, params, &op_data));

  switch (input->type) {
    case kTfLiteFloat32: {
      SoftmaxFloat(input, output, op_data);
      return kTfLiteOk;
    }
    case kTfLiteInt8:
    case kTfLiteUInt8: {
      SoftmaxQuantized(input, output, op_data);
      return kTfLiteOk;
    }
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s (%d) not supported.",
                         TfLiteTypeGetName(input->type), input->type);
      return kTfLiteError;
  }
}
}  // namespace activations

TfLiteRegistration* Register_SOFTMAX() {
  // TODO(b/149408647): Once we remove AddBuiltin from MicroOpResolver and
  // completely switch to the templated AddBuiltin from MicroMutableOpResolver,
  // this struct no longer needs to be static and can be returned by value.
  static TfLiteRegistration r = {/*init=*/nullptr,
                                 /*free=*/nullptr,
                                 /*prepare=*/activations::SoftmaxPrepare,
                                 /*invoke=*/activations::SoftmaxEval,
                                 /*profiling_string=*/nullptr,
                                 /*builtin_code=*/0,
                                 /*custom_name=*/nullptr,
                                 /*version=*/0};
  return &r;
}

}  // namespace micro
}  // namespace ops
}  // namespace tflite
