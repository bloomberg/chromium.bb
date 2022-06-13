/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/lite/kernels/internal/reference/pad.h"

#include <stdint.h>

#include <limits>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/internal/compatibility.h"
#include "tensorflow/lite/kernels/internal/optimized/optimized_ops.h"
#include "tensorflow/lite/kernels/internal/reference/reference_ops.h"
#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/internal/types.h"
#include "tensorflow/lite/kernels/kernel_util.h"

namespace tflite {
namespace ops {
namespace builtin {
namespace pad {

// This file has two implementations of Pad.
enum KernelType {
  kReference,
  kGenericOptimized,
};

struct PadContext {
  PadContext(TfLiteContext* context, TfLiteNode* node) {
    input = GetInput(context, node, 0);
    paddings = GetInput(context, node, 1);
    if (NumInputs(node) == 3) {
      constant_values = GetOptionalInputTensor(context, node, 2);
    } else {
      constant_values = nullptr;
    }
    output = GetOutput(context, node, 0);
    dims = NumDimensions(input);

    resizing_category = ResizingCategory::kGenericResize;
    const int paddings_total = GetTensorShape(paddings).FlatSize();
    const int32* paddings_data = GetTensorData<int32>(paddings);
    // Paddings will be a n,2 array, and we need to detect 4D arrays with the
    // pattern { {0,0}, {a, b}, {c, d}, {0,0} }.
    if (IsConstantTensor(paddings) && paddings_total == 8 &&
        (paddings_data[0] == 0 && paddings_data[1] == 0) &&
        (paddings_data[6] == 0 && paddings_data[7] == 0)) {
      resizing_category = ResizingCategory::kImageStyle;
    }
  }
  const TfLiteTensor* constant_values;
  const TfLiteTensor* input;
  const TfLiteTensor* paddings;
  TfLiteTensor* output;
  int dims;
  ResizingCategory resizing_category;
};

// Resizes output array based on the input size and padding size. This function
// is callable from both Prepare() and Eval() as long as the caller ensures the
// paddings data is present.
TfLiteStatus ResizeOutputTensor(TfLiteContext* context,
                                PadContext* op_context) {
  // Ensures the paddings array is dims x 2.
  TF_LITE_ENSURE_EQ(context, SizeOfDimension(op_context->paddings, 0),
                    op_context->dims);
  TF_LITE_ENSURE_EQ(context, SizeOfDimension(op_context->paddings, 1), 2);

  // Ensures all the elements of the paddings is non-negative.
  const int32* paddings_data = GetTensorData<int32>(op_context->paddings);

  for (int idx = 0; idx < op_context->dims; ++idx) {
    int before_padding = *paddings_data++;
    int after_padding = *paddings_data++;

    TF_LITE_ENSURE_MSG(context, (before_padding >= 0 && after_padding >= 0),
                       "Pad value has to be greater than equal to 0.");
  }

  // Determines the size of the output tensor.
  TfLiteIntArray* input_size = op_context->input->dims;
  TfLiteIntArray* output_size = TfLiteIntArrayCopy(input_size);
  paddings_data = GetTensorData<int32>(op_context->paddings);

  for (int idx = 0; idx < op_context->dims; ++idx) {
    int before_padding = *paddings_data++;
    int after_padding = *paddings_data++;

    output_size->data[idx] =
        (input_size->data[idx] + before_padding + after_padding);
  }

  return context->ResizeTensor(context, op_context->output, output_size);
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE(context, NumInputs(node) == 2 || NumInputs(node) == 3);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  PadContext op_context(context, node);
  TF_LITE_ENSURE_TYPES_EQ(context, op_context.input->type,
                          op_context.output->type);
  if (op_context.constant_values != nullptr) {
    TF_LITE_ENSURE_TYPES_EQ(context, op_context.input->type,
                            op_context.constant_values->type);
  }

  // Ensure we do not exceed maximum dimension count.
  TF_LITE_ENSURE(
      context, op_context.dims <= reference_ops::PadKernelMaxDimensionCount());

  // Exit early if paddings is a non-const tensor or the given input is an
  // unranked input. Set output tensor to dynamic so output size can be
  // determined in Eval.
  if (NumDimensions(op_context.input) == 0 ||
      !IsConstantTensor(op_context.paddings)) {
    SetTensorToDynamic(op_context.output);
    return kTfLiteOk;
  }
  return ResizeOutputTensor(context, &op_context);
}

template <typename integer_type>
TfLiteStatus EvalInt(TfLiteContext* context, const PadContext& op_context,
                     const tflite::PadParams& op_params) {
  integer_type pad_value;
  if (op_context.constant_values == nullptr) {
    // Quantized Pad requires that 0 is represented in the quantized
    // range.
    TF_LITE_ENSURE(context, op_context.output->params.zero_point >=
                                std::numeric_limits<integer_type>::min());
    TF_LITE_ENSURE(context, op_context.output->params.zero_point <=
                                std::numeric_limits<integer_type>::max());
    pad_value = static_cast<integer_type>(op_context.output->params.zero_point);
  } else {
    // Quantized Pad requires that 'constant_values' is represented in the
    // same quantized range as the input and output tensors.
    TF_LITE_ENSURE_EQ(context, op_context.output->params.zero_point,
                      op_context.constant_values->params.zero_point);
    TF_LITE_ENSURE_EQ(context, op_context.output->params.scale,
                      op_context.constant_values->params.scale);
    pad_value = *GetTensorData<integer_type>(op_context.constant_values);
  }
  const integer_type pad_value_copy = pad_value;
  if (op_context.resizing_category == ResizingCategory::kImageStyle) {
    optimized_ops::PadImageStyle(
        op_params, GetTensorShape(op_context.input),
        GetTensorData<integer_type>(op_context.input), &pad_value_copy,
        GetTensorShape(op_context.output),
        GetTensorData<integer_type>(op_context.output));
  } else {
    optimized_ops::Pad(op_params, GetTensorShape(op_context.input),
                       GetTensorData<integer_type>(op_context.input),
                       &pad_value_copy, GetTensorShape(op_context.output),
                       GetTensorData<integer_type>(op_context.output));
  }

  return kTfLiteOk;
}

template <KernelType kernel_type>
TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  PadContext op_context(context, node);

  if (op_context.constant_values != nullptr) {
    // Ensure that constant_values is a scalar.
    TF_LITE_ENSURE_EQ(context, NumElements(op_context.constant_values), 1);
  }

  // Resize the output tensor if the output tensor is dynamic.
  if (IsDynamicTensor(op_context.output)) {
    TF_LITE_ENSURE_OK(context, ResizeOutputTensor(context, &op_context));
  }

  // Create before and after padding arrays that are accepted by the kernel.
  const int32* paddings_data = GetTensorData<int32>(op_context.paddings);

  TF_LITE_ENSURE(
      context, op_context.dims <= reference_ops::PadKernelMaxDimensionCount());

  tflite::PadParams op_params;
  op_params.left_padding_count = op_context.dims;
  op_params.right_padding_count = op_context.dims;

  for (int idx = op_context.dims - 1; idx >= 0; --idx) {
    op_params.left_padding[idx] = paddings_data[idx * 2];
    op_params.right_padding[idx] = paddings_data[idx * 2 + 1];
  }

#define TF_LITE_PAD(type, op_name, scalar, pad_value)                     \
  const scalar pad_value_copy = pad_value;                                \
                                                                          \
  type::op_name(op_params, GetTensorShape(op_context.input),              \
                GetTensorData<scalar>(op_context.input), &pad_value_copy, \
                GetTensorShape(op_context.output),                        \
                GetTensorData<scalar>(op_context.output))
  switch (op_context.input->type) {
    case kTfLiteFloat32: {
      float pad_value = op_context.constant_values == nullptr
                            ? 0.f
                            : *GetTensorData<float>(op_context.constant_values);
      if (kernel_type == kReference) {
        if (op_context.resizing_category == ResizingCategory::kImageStyle) {
          TF_LITE_PAD(reference_ops, PadImageStyle, float, pad_value);
        } else {
          TF_LITE_PAD(reference_ops, Pad, float, pad_value);
        }
      } else if (kernel_type == kGenericOptimized) {
        if (op_context.resizing_category == ResizingCategory::kImageStyle) {
          TF_LITE_PAD(optimized_ops, PadImageStyle, float, pad_value);
        } else {
          TF_LITE_PAD(optimized_ops, Pad, float, pad_value);
        }
      }
    } break;
    case kTfLiteUInt8: {
      EvalInt<uint8_t>(context, op_context, op_params);
    } break;
    case kTfLiteInt8: {
      EvalInt<int8_t>(context, op_context, op_params);
    } break;
    case kTfLiteInt16: {
      EvalInt<int16_t>(context, op_context, op_params);
    } break;
    case kTfLiteInt32: {
      int32_t pad_value =
          op_context.constant_values == nullptr
              ? 0
              : *GetTensorData<int32_t>(op_context.constant_values);
      if (kernel_type == kReference) {
        TF_LITE_PAD(reference_ops, Pad, int32_t, pad_value);
      } else if (kernel_type == kGenericOptimized) {
        TF_LITE_PAD(optimized_ops, Pad, int32_t, pad_value);
      }
    } break;
    case kTfLiteInt64: {
      int64_t pad_value =
          op_context.constant_values == nullptr
              ? 0L
              : *GetTensorData<int64_t>(op_context.constant_values);
      if (kernel_type == kReference) {
        TF_LITE_PAD(reference_ops, Pad, int64_t, pad_value);
      } else if (kernel_type == kGenericOptimized) {
        TF_LITE_PAD(optimized_ops, Pad, int64_t, pad_value);
      }
    } break;
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s is currently not supported by Pad.",
                         TfLiteTypeGetName(op_context.input->type));
      return kTfLiteError;
  }
#undef TF_LITE_PAD
  return kTfLiteOk;
}

}  // namespace pad

TfLiteRegistration* Register_PAD_REF() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kReference>};
  return &r;
}

TfLiteRegistration* Register_PAD_GENERIC_OPT() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kGenericOptimized>};
  return &r;
}

TfLiteRegistration* Register_PAD() { return Register_PAD_GENERIC_OPT(); }

// Also register Pad as PadV2.
TfLiteRegistration* Register_PADV2_REF() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kReference>};
  return &r;
}

TfLiteRegistration* Register_PADV2_GENERIC_OPT() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kGenericOptimized>};
  return &r;
}

TfLiteRegistration* Register_PADV2() { return Register_PADV2_GENERIC_OPT(); }

}  // namespace builtin
}  // namespace ops
}  // namespace tflite
