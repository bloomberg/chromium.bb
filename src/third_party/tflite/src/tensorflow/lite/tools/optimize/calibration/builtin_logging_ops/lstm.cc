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
#include "tensorflow/lite/tools/optimize/calibration/builtin_logging_ops/lstm.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/core/api/error_reporter.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/internal/kernel_utils.h"
#include "tensorflow/lite/kernels/internal/optimized/optimized_ops.h"
#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/internal/tensor_utils.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/kernels/lstm_shared.h"
#include "tensorflow/lite/kernels/op_macros.h"
#include "tensorflow/lite/tools/optimize/calibration/calibration_logger.h"

namespace tflite {
namespace optimize {
namespace calibration {
namespace builtin {

namespace {

inline void LstmStepWithAuxInput(
    const float* input_ptr, const float* input_to_input_weights_ptr,
    const float* input_to_forget_weights_ptr,
    const float* input_to_cell_weights_ptr,
    const float* input_to_output_weights_ptr, const float* aux_input_ptr,
    const float* aux_input_to_input_weights_ptr,
    const float* aux_input_to_forget_weights_ptr,
    const float* aux_input_to_cell_weights_ptr,
    const float* aux_input_to_output_weights_ptr,
    const float* recurrent_to_input_weights_ptr,
    const float* recurrent_to_forget_weights_ptr,
    const float* recurrent_to_cell_weights_ptr,
    const float* recurrent_to_output_weights_ptr,
    const float* cell_to_input_weights_ptr,
    const float* cell_to_forget_weights_ptr,
    const float* cell_to_output_weights_ptr,
    const float* input_layer_norm_coefficients_ptr,
    const float* forget_layer_norm_coefficients_ptr,
    const float* cell_layer_norm_coefficients_ptr,
    const float* output_layer_norm_coefficients_ptr,
    const float* input_gate_bias_ptr, const float* forget_gate_bias_ptr,
    const float* cell_gate_bias_ptr, const float* output_gate_bias_ptr,
    const float* projection_weights_ptr, const float* projection_bias_ptr,
    const TfLiteLSTMParams* params, int n_batch, int n_cell, int n_input,
    int n_aux_input, int n_output, int output_batch_leading_dim,
    float* output_state_ptr, float* cell_state_ptr, float* scratch0,
    float* scratch1, float* scratch2, float* scratch3, float* output_ptr,
    Logger* logger, const std::vector<int>& intermediate_tensor_indexes,
    ErrorReporter* error_reporter) {
  // Make named scratch buffers for the different gates.
  float* input_gate_scratch = scratch0;
  float* forget_gate_scratch = scratch1;
  float* cell_gate_scratch = scratch2;
  float* output_gate_scratch = scratch3;

  // Since we have already checked that weights are all there or none, we can
  // check the existence of only one to the get the condition.
  const bool use_cifg = (input_to_input_weights_ptr == nullptr);
  const bool use_peephole = (cell_to_output_weights_ptr != nullptr);
  const bool use_layer_norm = (forget_layer_norm_coefficients_ptr != nullptr);

  // Initialize scratch buffers with bias for regular lstm or initialize with
  // zero for layer norm lstm.
  if (use_layer_norm) {
    if (!use_cifg) {
      std::fill_n(input_gate_scratch, n_cell * n_batch, 0.0f);
    }
    std::fill_n(forget_gate_scratch, n_cell * n_batch, 0.0f);
    std::fill_n(cell_gate_scratch, n_cell * n_batch, 0.0f);
    std::fill_n(output_gate_scratch, n_cell * n_batch, 0.0f);
  } else {
    if (!use_cifg) {
      tensor_utils::VectorBatchVectorAssign(input_gate_bias_ptr, n_cell,
                                            n_batch, input_gate_scratch);
    }
    tensor_utils::VectorBatchVectorAssign(forget_gate_bias_ptr, n_cell, n_batch,
                                          forget_gate_scratch);
    tensor_utils::VectorBatchVectorAssign(cell_gate_bias_ptr, n_cell, n_batch,
                                          cell_gate_scratch);
    tensor_utils::VectorBatchVectorAssign(output_gate_bias_ptr, n_cell, n_batch,
                                          output_gate_scratch);
  }

  // For each batch and cell: compute input_weight * input.
  if (!use_cifg) {
    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        input_to_input_weights_ptr, n_cell, n_input, input_ptr, n_batch,
        input_gate_scratch);
  }

  tensor_utils::MatrixBatchVectorMultiplyAccumulate(
      input_to_forget_weights_ptr, n_cell, n_input, input_ptr, n_batch,
      forget_gate_scratch);
  tensor_utils::MatrixBatchVectorMultiplyAccumulate(input_to_cell_weights_ptr,
                                                    n_cell, n_input, input_ptr,
                                                    n_batch, cell_gate_scratch);
  tensor_utils::MatrixBatchVectorMultiplyAccumulate(
      input_to_output_weights_ptr, n_cell, n_input, input_ptr, n_batch,
      output_gate_scratch);

  // If auxiliary input is available then compute aux_input_weight * aux_input
  if (aux_input_ptr != nullptr) {
    if (!use_cifg) {
      tensor_utils::MatrixBatchVectorMultiplyAccumulate(
          aux_input_to_input_weights_ptr, n_cell, n_aux_input, aux_input_ptr,
          n_batch, input_gate_scratch);
    }

    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        aux_input_to_forget_weights_ptr, n_cell, n_aux_input, aux_input_ptr,
        n_batch, forget_gate_scratch);
    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        aux_input_to_cell_weights_ptr, n_cell, n_aux_input, aux_input_ptr,
        n_batch, cell_gate_scratch);
    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        aux_input_to_output_weights_ptr, n_cell, n_aux_input, aux_input_ptr,
        n_batch, output_gate_scratch);
  }

  // For each batch and cell: compute recurrent_weight * output_state.
  if (!use_cifg) {
    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        recurrent_to_input_weights_ptr, n_cell, n_output, output_state_ptr,
        n_batch, input_gate_scratch);
  }
  tensor_utils::MatrixBatchVectorMultiplyAccumulate(
      recurrent_to_forget_weights_ptr, n_cell, n_output, output_state_ptr,
      n_batch, forget_gate_scratch);
  tensor_utils::MatrixBatchVectorMultiplyAccumulate(
      recurrent_to_cell_weights_ptr, n_cell, n_output, output_state_ptr,
      n_batch, cell_gate_scratch);
  tensor_utils::MatrixBatchVectorMultiplyAccumulate(
      recurrent_to_output_weights_ptr, n_cell, n_output, output_state_ptr,
      n_batch, output_gate_scratch);

  // For each batch and cell: update input gate.
  if (!use_cifg) {
    if (use_peephole) {
      tensor_utils::VectorBatchVectorCwiseProductAccumulate(
          cell_to_input_weights_ptr, n_cell, cell_state_ptr, n_batch,
          input_gate_scratch);
    }
    if (use_layer_norm) {
      logger->LogTensorValue(intermediate_tensor_indexes[0], input_gate_scratch,
                             n_cell * n_batch, error_reporter);
      tensor_utils::MeanStddevNormalization(
          input_gate_scratch, input_gate_scratch, n_cell, n_batch);
      tensor_utils::VectorBatchVectorCwiseProduct(
          input_layer_norm_coefficients_ptr, n_cell, input_gate_scratch,
          n_batch, input_gate_scratch);
      tensor_utils::VectorBatchVectorAdd(input_gate_bias_ptr, n_cell, n_batch,
                                         input_gate_scratch);
    }
    tensor_utils::ApplySigmoidToVector(input_gate_scratch, n_cell * n_batch,
                                       input_gate_scratch);
  }

  // For each batch and cell: update forget gate.
  if (use_peephole) {
    tensor_utils::VectorBatchVectorCwiseProductAccumulate(
        cell_to_forget_weights_ptr, n_cell, cell_state_ptr, n_batch,
        forget_gate_scratch);
  }
  if (use_layer_norm) {
    logger->LogTensorValue(intermediate_tensor_indexes[1], forget_gate_scratch,
                           n_cell * n_batch, error_reporter);
    tensor_utils::MeanStddevNormalization(forget_gate_scratch,
                                          forget_gate_scratch, n_cell, n_batch);
    tensor_utils::VectorBatchVectorCwiseProduct(
        forget_layer_norm_coefficients_ptr, n_cell, forget_gate_scratch,
        n_batch, forget_gate_scratch);
    tensor_utils::VectorBatchVectorAdd(forget_gate_bias_ptr, n_cell, n_batch,
                                       forget_gate_scratch);
  }
  tensor_utils::ApplySigmoidToVector(forget_gate_scratch, n_cell * n_batch,
                                     forget_gate_scratch);

  // For each batch and cell: update the cell.
  tensor_utils::VectorVectorCwiseProduct(forget_gate_scratch, cell_state_ptr,
                                         n_batch * n_cell, cell_state_ptr);
  if (use_layer_norm) {
    logger->LogTensorValue(intermediate_tensor_indexes[2], cell_gate_scratch,
                           n_cell * n_batch, error_reporter);
    tensor_utils::MeanStddevNormalization(cell_gate_scratch, cell_gate_scratch,
                                          n_cell, n_batch);
    tensor_utils::VectorBatchVectorCwiseProduct(
        cell_layer_norm_coefficients_ptr, n_cell, cell_gate_scratch, n_batch,
        cell_gate_scratch);
    tensor_utils::VectorBatchVectorAdd(cell_gate_bias_ptr, n_cell, n_batch,
                                       cell_gate_scratch);
  }
  tensor_utils::ApplyActivationToVector(cell_gate_scratch, n_batch * n_cell,
                                        params->activation, cell_gate_scratch);
  if (use_cifg) {
    tensor_utils::Sub1Vector(forget_gate_scratch, n_batch * n_cell,
                             forget_gate_scratch);
    tensor_utils::VectorVectorCwiseProductAccumulate(
        cell_gate_scratch, forget_gate_scratch, n_batch * n_cell,
        cell_state_ptr);
  } else {
    tensor_utils::VectorVectorCwiseProductAccumulate(
        cell_gate_scratch, input_gate_scratch, n_batch * n_cell,
        cell_state_ptr);
  }
  if (params->cell_clip > 0.0) {
    tensor_utils::ClipVector(cell_state_ptr, n_batch * n_cell,
                             params->cell_clip, cell_state_ptr);
  }

  // For each batch and cell: update the output gate.
  if (use_peephole) {
    tensor_utils::VectorBatchVectorCwiseProductAccumulate(
        cell_to_output_weights_ptr, n_cell, cell_state_ptr, n_batch,
        output_gate_scratch);
  }
  if (use_layer_norm) {
    logger->LogTensorValue(intermediate_tensor_indexes[3], output_gate_scratch,
                           n_cell * n_batch, error_reporter);
    tensor_utils::MeanStddevNormalization(output_gate_scratch,
                                          output_gate_scratch, n_cell, n_batch);
    tensor_utils::VectorBatchVectorCwiseProduct(
        output_layer_norm_coefficients_ptr, n_cell, output_gate_scratch,
        n_batch, output_gate_scratch);
    tensor_utils::VectorBatchVectorAdd(output_gate_bias_ptr, n_cell, n_batch,
                                       output_gate_scratch);
  }
  tensor_utils::ApplySigmoidToVector(output_gate_scratch, n_batch * n_cell,
                                     output_gate_scratch);
  tensor_utils::ApplyActivationToVector(cell_state_ptr, n_batch * n_cell,
                                        params->activation, cell_gate_scratch);
  tensor_utils::VectorVectorCwiseProduct(output_gate_scratch, cell_gate_scratch,
                                         n_batch * n_cell, output_gate_scratch);

  logger->LogTensorValue(intermediate_tensor_indexes[4], output_gate_scratch,
                         n_cell * n_batch, error_reporter);

  const bool use_projection_weight = (projection_weights_ptr != nullptr);
  const bool use_projection_bias = (projection_bias_ptr != nullptr);

  // For each batch: update output_state.
  if (use_projection_weight) {
    if (use_projection_bias) {
      tensor_utils::VectorBatchVectorAssign(projection_bias_ptr, n_output,
                                            n_batch, output_state_ptr);
    } else {
      std::fill_n(output_state_ptr, n_batch * n_output, 0.0f);
    }
    tensor_utils::MatrixBatchVectorMultiplyAccumulate(
        projection_weights_ptr, n_output, n_cell, output_gate_scratch, n_batch,
        output_state_ptr);
    if (params->proj_clip > 0.0) {
      tensor_utils::ClipVector(output_state_ptr, n_batch * n_output,
                               params->proj_clip, output_state_ptr);
    }
  } else {
    std::copy_n(output_gate_scratch, n_batch * n_output, output_state_ptr);
  }
  // Copy output_state to the output. Note that the output batch rows may not be
  // contiguous (output_batch_leading_dim != n_output).
  for (int b = 0; b < n_batch; b++) {
    std::copy_n(output_state_ptr + b * n_output, n_output,
                output_ptr + b * output_batch_leading_dim);
  }
}

TfLiteStatus EvalFloat(
    const TfLiteTensor* input, const TfLiteTensor* input_to_input_weights,
    const TfLiteTensor* input_to_forget_weights,
    const TfLiteTensor* input_to_cell_weights,
    const TfLiteTensor* input_to_output_weights,
    const TfLiteTensor* recurrent_to_input_weights,
    const TfLiteTensor* recurrent_to_forget_weights,
    const TfLiteTensor* recurrent_to_cell_weights,
    const TfLiteTensor* recurrent_to_output_weights,
    const TfLiteTensor* cell_to_input_weights,
    const TfLiteTensor* cell_to_forget_weights,
    const TfLiteTensor* cell_to_output_weights,
    const TfLiteTensor* input_layer_norm_coefficients,
    const TfLiteTensor* forget_layer_norm_coefficients,
    const TfLiteTensor* cell_layer_norm_coefficients,
    const TfLiteTensor* output_layer_norm_coefficients,
    const TfLiteTensor* aux_input,
    const TfLiteTensor* aux_input_to_input_weights,
    const TfLiteTensor* aux_input_to_forget_weights,
    const TfLiteTensor* aux_input_to_cell_weights,
    const TfLiteTensor* aux_input_to_output_weights,
    const TfLiteTensor* input_gate_bias, const TfLiteTensor* forget_gate_bias,
    const TfLiteTensor* cell_gate_bias, const TfLiteTensor* output_gate_bias,
    const TfLiteTensor* projection_weights, const TfLiteTensor* projection_bias,
    const TfLiteLSTMParams* params, bool forward_sequence, bool time_major,
    int output_offset, TfLiteTensor* scratch_buffer, TfLiteTensor* output_state,
    TfLiteTensor* cell_state, TfLiteTensor* output, Logger* logger,
    const std::vector<int>& intermediate_tensor_indexes,
    ErrorReporter* error_reporter) {
  TF_LITE_ASSERT(input->dims->size >= 2 && input->dims->size <= 3);
  int max_time, n_batch;
  if (input->dims->size == 3) {
    max_time = (time_major) ? input->dims->data[0] : input->dims->data[1];
    n_batch = (time_major) ? input->dims->data[1] : input->dims->data[0];
  } else {
    max_time = 1;
    n_batch = input->dims->data[0];
  }
  const int n_input = input->dims->data[input->dims->size - 1];
  const int aux_input_size =
      (aux_input) ? aux_input->dims->data[aux_input->dims->size - 1] : 0;

  // n_cell and n_output will be the same size when there is no projection.
  const int n_cell = input_to_output_weights->dims->data[0];
  const int n_output = recurrent_to_output_weights->dims->data[1];

  // Since we have already checked that weights are all there or none, we can
  // check the existence of only one to the get the condition.
  const bool use_cifg = (input_to_input_weights == nullptr);

  // Index the scratch buffers pointers to the global scratch buffer.
  float* scratch_buffer_ptr = GetTensorData<float>(scratch_buffer);
  float* input_gate_scratch = nullptr;
  float* cell_gate_scratch = nullptr;
  float* forget_gate_scratch = nullptr;
  float* output_gate_scratch = nullptr;
  if (use_cifg) {
    cell_gate_scratch = scratch_buffer_ptr;
    forget_gate_scratch = scratch_buffer_ptr + n_cell * n_batch;
    output_gate_scratch = scratch_buffer_ptr + 2 * n_cell * n_batch;
  } else {
    input_gate_scratch = scratch_buffer_ptr;
    cell_gate_scratch = scratch_buffer_ptr + n_cell * n_batch;
    forget_gate_scratch = scratch_buffer_ptr + 2 * n_cell * n_batch;
    output_gate_scratch = scratch_buffer_ptr + 3 * n_cell * n_batch;
  }

  const int output_batch_leading_dim =
      output->dims->data[output->dims->size - 1];
  if (time_major) {
    // Loop through the sequence.
    const int input_step = n_batch * n_input;
    const int output_step = n_batch * output_batch_leading_dim;
    for (int t = 0; t < max_time; t++) {
      // If this is the forward_sequence, step forward, otherwise step
      // backwards.
      const int t_rel = forward_sequence ? t : max_time - t - 1;
      const float* input_ptr = GetTensorData<float>(input) + t_rel * input_step;
      const float* aux_input_ptr = nullptr;
      if (aux_input) {
        aux_input_ptr = GetTensorData<float>(aux_input) + t_rel * input_step;
      }
      float* output_ptr_time =
          GetTensorData<float>(output) + t_rel * output_step + output_offset;

      LstmStepWithAuxInput(
          input_ptr, GetTensorData<float>(input_to_input_weights),
          GetTensorData<float>(input_to_forget_weights),
          GetTensorData<float>(input_to_cell_weights),
          GetTensorData<float>(input_to_output_weights), aux_input_ptr,
          GetTensorData<float>(aux_input_to_input_weights),
          GetTensorData<float>(aux_input_to_forget_weights),
          GetTensorData<float>(aux_input_to_cell_weights),
          GetTensorData<float>(aux_input_to_output_weights),
          GetTensorData<float>(recurrent_to_input_weights),
          GetTensorData<float>(recurrent_to_forget_weights),
          GetTensorData<float>(recurrent_to_cell_weights),
          GetTensorData<float>(recurrent_to_output_weights),
          GetTensorData<float>(cell_to_input_weights),
          GetTensorData<float>(cell_to_forget_weights),
          GetTensorData<float>(cell_to_output_weights),
          GetTensorData<float>(input_layer_norm_coefficients),
          GetTensorData<float>(forget_layer_norm_coefficients),
          GetTensorData<float>(cell_layer_norm_coefficients),
          GetTensorData<float>(output_layer_norm_coefficients),
          GetTensorData<float>(input_gate_bias),
          GetTensorData<float>(forget_gate_bias),
          GetTensorData<float>(cell_gate_bias),
          GetTensorData<float>(output_gate_bias),
          GetTensorData<float>(projection_weights),
          GetTensorData<float>(projection_bias), params, n_batch, n_cell,
          n_input, aux_input_size, n_output, output_batch_leading_dim,
          GetTensorData<float>(output_state), GetTensorData<float>(cell_state),
          input_gate_scratch, forget_gate_scratch, cell_gate_scratch,
          output_gate_scratch, output_ptr_time, logger,
          intermediate_tensor_indexes, error_reporter);
    }
  } else {
    for (int b = 0; b < n_batch; b++) {
      const int input_step = n_input;
      const int output_step = output_batch_leading_dim;
      for (int t = 0; t < max_time; t++) {
        // If this is the forward_sequence, step forward, otherwise step
        // backwards.
        const int t_rel = forward_sequence ? t : max_time - t - 1;
        const int time_offset = b * max_time + t_rel;
        const float* input_ptr =
            GetTensorData<float>(input) + time_offset * input_step;
        const float* aux_input_ptr = nullptr;
        if (aux_input) {
          aux_input_ptr =
              GetTensorData<float>(aux_input) + time_offset * input_step;
        }
        float* output_ptr = GetTensorData<float>(output) +
                            time_offset * output_step + output_offset;

        // Offset the {output,cell}_state pointers to the right batch.
        float* output_state_ptr =
            GetTensorData<float>(output_state) + b * output_batch_leading_dim;
        float* cell_state_ptr = GetTensorData<float>(cell_state) + b * n_cell;
        // Offset the scratch pointers to the right batch.
        float* input_gate_scratch_ptr =
            input_gate_scratch ? input_gate_scratch + b * n_cell : nullptr;
        float* forget_gate_scratch_ptr = forget_gate_scratch + b * n_cell;
        float* cell_gate_scratch_ptr = cell_gate_scratch + b * n_cell;
        float* output_gate_scratch_ptr = output_gate_scratch + b * n_cell;

        LstmStepWithAuxInput(
            input_ptr, GetTensorData<float>(input_to_input_weights),
            GetTensorData<float>(input_to_forget_weights),
            GetTensorData<float>(input_to_cell_weights),
            GetTensorData<float>(input_to_output_weights), aux_input_ptr,
            GetTensorData<float>(aux_input_to_input_weights),
            GetTensorData<float>(aux_input_to_forget_weights),
            GetTensorData<float>(aux_input_to_cell_weights),
            GetTensorData<float>(aux_input_to_output_weights),
            GetTensorData<float>(recurrent_to_input_weights),
            GetTensorData<float>(recurrent_to_forget_weights),
            GetTensorData<float>(recurrent_to_cell_weights),
            GetTensorData<float>(recurrent_to_output_weights),
            GetTensorData<float>(cell_to_input_weights),
            GetTensorData<float>(cell_to_forget_weights),
            GetTensorData<float>(cell_to_output_weights),
            GetTensorData<float>(input_layer_norm_coefficients),
            GetTensorData<float>(forget_layer_norm_coefficients),
            GetTensorData<float>(cell_layer_norm_coefficients),
            GetTensorData<float>(output_layer_norm_coefficients),
            GetTensorData<float>(input_gate_bias),
            GetTensorData<float>(forget_gate_bias),
            GetTensorData<float>(cell_gate_bias),
            GetTensorData<float>(output_gate_bias),
            GetTensorData<float>(projection_weights),
            GetTensorData<float>(projection_bias), params, /*n_batch=*/1,
            n_cell, n_input, aux_input_size, n_output, output_batch_leading_dim,
            output_state_ptr, cell_state_ptr, input_gate_scratch_ptr,
            forget_gate_scratch_ptr, cell_gate_scratch_ptr,
            output_gate_scratch_ptr, output_ptr, logger,
            intermediate_tensor_indexes, error_reporter);
      }
    }
  }
  return kTfLiteOk;
}

struct OpData {
  // Which kernel type to use. Full kernel (24 inputs) or basic kernel (5
  // inputs).
  // Please note the 20-input full kernel is deprecated and only kept
  // here for backward compatibility.
  TfLiteLSTMKernelType kernel_type;

  // If the lstm is layer norm.
  bool use_layer_norm;

  // These fields are only used by full kernel.
  int scratch_tensor_index;
};

// Resize the output, state tensors based on the sizes of the input tensors.
// Allocate a temporary scratch tensor. Also check that the sizes of the input
// tensors match each other.
TfLiteStatus lstm_eval(TfLiteContext* context, TfLiteNode* node, Logger* logger,
                       ErrorReporter* error_reporter) {
  const auto* params = static_cast<TfLiteLSTMParams*>(node->builtin_data);

  const TfLiteTensor* input =
      GetInput(context, node, ops::builtin::lstm::full::kInputTensor);

  const TfLiteTensor* input_to_input_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kInputToInputWeightsTensor);
  const TfLiteTensor* input_to_forget_weights = GetInput(
      context, node, ops::builtin::lstm::full::kInputToForgetWeightsTensor);
  const TfLiteTensor* input_to_cell_weights = GetInput(
      context, node, ops::builtin::lstm::full::kInputToCellWeightsTensor);
  const TfLiteTensor* input_to_output_weights = GetInput(
      context, node, ops::builtin::lstm::full::kInputToOutputWeightsTensor);

  const TfLiteTensor* recurrent_to_input_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kRecurrentToInputWeightsTensor);
  const TfLiteTensor* recurrent_to_forget_weights = GetInput(
      context, node, ops::builtin::lstm::full::kRecurrentToForgetWeightsTensor);
  const TfLiteTensor* recurrent_to_cell_weights = GetInput(
      context, node, ops::builtin::lstm::full::kRecurrentToCellWeightsTensor);
  const TfLiteTensor* recurrent_to_output_weights = GetInput(
      context, node, ops::builtin::lstm::full::kRecurrentToOutputWeightsTensor);

  const TfLiteTensor* cell_to_input_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kCellToInputWeightsTensor);
  const TfLiteTensor* cell_to_forget_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kCellToForgetWeightsTensor);
  const TfLiteTensor* cell_to_output_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kCellToOutputWeightsTensor);

  const TfLiteTensor* input_layer_norm_coefficients = GetOptionalInputTensor(
      context, node,
      ops::builtin::lstm::full::kInputLayerNormCoefficientsTensor);
  const TfLiteTensor* forget_layer_norm_coefficients = GetOptionalInputTensor(
      context, node,
      ops::builtin::lstm::full::kForgetLayerNormCoefficientsTensor);
  const TfLiteTensor* cell_layer_norm_coefficients = GetOptionalInputTensor(
      context, node,
      ops::builtin::lstm::full::kCellLayerNormCoefficientsTensor);
  const TfLiteTensor* output_layer_norm_coefficients = GetOptionalInputTensor(
      context, node,
      ops::builtin::lstm::full::kOutputLayerNormCoefficientsTensor);

  const TfLiteTensor* input_gate_bias = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kInputGateBiasTensor);
  const TfLiteTensor* forget_gate_bias =
      GetInput(context, node, ops::builtin::lstm::full::kForgetGateBiasTensor);
  const TfLiteTensor* cell_gate_bias =
      GetInput(context, node, ops::builtin::lstm::full::kCellGateBiasTensor);
  const TfLiteTensor* output_gate_bias =
      GetInput(context, node, ops::builtin::lstm::full::kOutputGateBiasTensor);

  const TfLiteTensor* projection_weights = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kProjectionWeightsTensor);
  const TfLiteTensor* projection_bias = GetOptionalInputTensor(
      context, node, ops::builtin::lstm::full::kProjectionBiasTensor);

  // Index the scratch buffers pointers to the global scratch buffer.
  TfLiteTensor* scratch_buffer = GetTemporary(context, node, /*index=*/0);

  TfLiteTensor* output_state = GetVariableInput(
      context, node, ops::builtin::lstm::full::kOutputStateTensor);
  TF_LITE_ENSURE(context, output_state != nullptr);
  TfLiteTensor* cell_state = GetVariableInput(
      context, node, ops::builtin::lstm::full::kCellStateTensor);
  TF_LITE_ENSURE(context, cell_state != nullptr);

  TfLiteTensor* output =
      GetOutput(context, node, ops::builtin::lstm::full::kOutputTensor);

  std::vector<int> intermediate_tensor_indexes(node->intermediates->size);
  for (int i = 0; i < node->intermediates->size; ++i) {
    intermediate_tensor_indexes[i] = node->intermediates->data[i];
  }

  switch (input_to_output_weights->type) {
    case kTfLiteFloat32: {
      return EvalFloat(
          input, input_to_input_weights, input_to_forget_weights,
          input_to_cell_weights, input_to_output_weights,
          recurrent_to_input_weights, recurrent_to_forget_weights,
          recurrent_to_cell_weights, recurrent_to_output_weights,
          cell_to_input_weights, cell_to_forget_weights, cell_to_output_weights,
          input_layer_norm_coefficients, forget_layer_norm_coefficients,
          cell_layer_norm_coefficients, output_layer_norm_coefficients,
          /*aux_input=*/nullptr,
          /*aux_input_to_input_weights=*/nullptr,
          /*aux_input_to_forget_weights=*/nullptr,
          /*aux_input_to_cell_weights=*/nullptr,
          /*aux_input_to_output_weights=*/nullptr, input_gate_bias,
          forget_gate_bias, cell_gate_bias, output_gate_bias,
          projection_weights, projection_bias, params,
          /*forward_sequence=*/true,
          /*time_major=*/true,
          /*output_offset=*/0, scratch_buffer, output_state, cell_state, output,
          logger, intermediate_tensor_indexes, error_reporter);
    }
    case kTfLiteUInt8:
    case kTfLiteInt8:
    default:
      printf("Error. Only float model can be calibrated\n");
      return kTfLiteError;
  }
  return kTfLiteOk;
}
}  // namespace

TfLiteStatus lstm_logging_kernel(TfLiteContext* context, TfLiteNode* node,
                                 Logger* logger,
                                 ErrorReporter* error_reporter) {
  return lstm_eval(context, node, logger, error_reporter);
}

}  // namespace builtin
}  // namespace calibration
}  // namespace optimize
}  // namespace tflite
