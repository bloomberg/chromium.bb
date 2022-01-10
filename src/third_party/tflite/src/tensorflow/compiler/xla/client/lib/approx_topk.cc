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

#include "tensorflow/compiler/xla/client/lib/approx_topk.h"

#include <limits>
#include <string>

#include "absl/strings/str_format.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/client/xla_computation.h"
#include "tensorflow/compiler/xla/shape.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/lib/core/bits.h"

// Used by rank 2+ operands
const uint64_t kTpuLaneTiling = 128;
// Used by rank 1 operands.
const uint64_t kTpuChunkTiling = 1024;

namespace xla {

// Converts a comparator to a combiner computation that can be fed to reduce or
// partial reduce ops.
XlaComputation BuildReductionComputation(
    XlaBuilder* builder, absl::Span<const PrimitiveType> op_types,
    const XlaComputation& comparator) {
  auto num_operands = op_types.size();
  std::vector<XlaOp> lhs_params;
  std::vector<XlaOp> rhs_params;
  int64_t param_number = 0;
  lhs_params.reserve(num_operands);
  rhs_params.reserve(num_operands);
  auto reduction_builder = builder->CreateSubBuilder("ReductionFn");
  for (const auto& op_type : op_types) {
    lhs_params.push_back(Parameter(reduction_builder.get(), param_number,
                                   ShapeUtil::MakeScalarShape(op_type),
                                   absl::StrFormat("lhs.%d", param_number)));
    param_number++;
  }
  for (const auto& op_type : op_types) {
    rhs_params.push_back(Parameter(reduction_builder.get(), param_number,
                                   ShapeUtil::MakeScalarShape(op_type),
                                   absl::StrFormat("rhs.%d", param_number)));
    param_number++;
  }

  std::vector<XlaOp> comparator_args;
  comparator_args.reserve(num_operands * 2);
  for (int i = 0; i < num_operands; ++i) {
    comparator_args.push_back(lhs_params[i]);
    comparator_args.push_back(rhs_params[i]);
  }
  auto pred = Call(reduction_builder.get(), comparator, comparator_args);
  std::vector<XlaOp> results;
  results.reserve(num_operands);
  for (int i = 0; i < num_operands; ++i) {
    results.push_back(Select(pred, lhs_params[i], rhs_params[i]));
  }
  Tuple(reduction_builder.get(), results);
  return reduction_builder->BuildAndNoteError();
}

XlaOp SortAndSliceBuilder(XlaBuilder* builder, absl::Span<const XlaOp> operands,
                          int64_t top_k, int64_t reduction_dim,
                          const XlaComputation& comparator) {
  auto operands_shapes = builder->GetOperandShapes(operands).ValueOrDie();
  int64_t rank = operands_shapes[0].rank();
  int64_t num_operands = operands.size();

  auto sorted_results = Sort(operands, comparator, reduction_dim);
  std::vector<int64_t> slice_start_indices(rank, 0);
  std::vector<int64_t> slice_limit_indices;
  std::vector<int64_t> slice_strides(rank, 1);
  slice_limit_indices.insert(slice_limit_indices.begin(),
                             operands_shapes[0].dimensions().begin(),
                             operands_shapes[0].dimensions().end());
  slice_limit_indices[reduction_dim] = top_k;

  std::vector<XlaOp> sliced_results;
  sliced_results.reserve(num_operands);
  for (int i = 0; i < num_operands; ++i) {
    sliced_results.push_back(Slice(GetTupleElement(sorted_results, i),
                                   slice_start_indices, slice_limit_indices,
                                   slice_strides));
  }
  return Tuple(builder, sliced_results);
}

XlaOp ApproxTopK(XlaBuilder* builder, absl::Span<const XlaOp> operands,
                 absl::Span<const XlaOp> init_values, int64_t top_k,
                 int64_t reduction_dim, const XlaComputation& comparator,
                 float recall_target, bool aggregate_to_topk,
                 int64_t reduction_input_size_override) {
  // Validates shapes and ranks
  if (operands.size() != init_values.size()) {
    return builder->ReportError(
        InvalidArgument("operands and init_values size mismatch: %d vs %d",
                        operands.size(), init_values.size()));
  }
  auto num_operands = operands.size();
  auto operands_shapes = builder->GetOperandShapes(operands).ValueOrDie();
  auto init_values_shapes = builder->GetOperandShapes(init_values).ValueOrDie();
  std::vector<PrimitiveType> op_types;
  for (int i = 0; i < num_operands; ++i) {
    const auto& op_shape = operands_shapes[i];
    const auto& init_shape = init_values_shapes[i];
    if (op_shape.rank() == 0) {
      return builder->ReportError(
          InvalidArgument("ApproxTopK operands must have rank 1+."));
    }
    if (!ShapeUtil::CompatibleIgnoringElementType(operands_shapes[0],
                                                  op_shape)) {
      return builder->ReportError(InvalidArgument(
          "operands shape mismatch: %s vs %s", operands_shapes[0].DebugString(),
          op_shape.DebugString()));
    }
    if (op_shape.element_type() != init_shape.element_type()) {
      return builder->ReportError(
          InvalidArgument("operands type mismatch: %s vs %s",
                          op_shape.DebugString(), init_shape.DebugString()));
    }

    op_types.push_back(op_shape.element_type());
  }
  int64_t rank = operands_shapes[0].rank();
  if (reduction_dim < 0 || reduction_dim >= rank) {
    return builder->ReportError(
        InvalidArgument("reduction_dim should range in [0,%d)", rank));
  }

  auto reduction_computation =
      BuildReductionComputation(builder, op_types, comparator);

  // Fallback to variadic reduce when top_k == 1.
  // TODO(fchern): Approx-topk followed by variadic reduce might run faster
  // than running variadic reduce directly.
  if (top_k == 1) {
    auto val_args = Reduce(builder, operands, init_values,
                           reduction_computation, {reduction_dim});
    Shape op_shape = operands_shapes[0];
    op_shape.mutable_dimensions()[reduction_dim] = 1;
    auto top1_vals =
        Reshape(GetTupleElement(val_args, 0), op_shape.dimensions());
    auto top1_args =
        Reshape(GetTupleElement(val_args, 1), op_shape.dimensions());
    return Tuple(builder, {top1_vals, top1_args});
  }

  uint64_t tpu_tiling = rank == 1 ? kTpuChunkTiling : kTpuLaneTiling;
  uint64_t n = operands_shapes[0].dimensions(reduction_dim);
  // ApproxTopK can only reduce elements larger than the tiling.
  if (n <= tpu_tiling) {
    if (aggregate_to_topk) {
      return SortAndSliceBuilder(builder, operands, top_k, reduction_dim,
                                 comparator);
    }
    return Tuple(builder, operands);
  }

  auto status_or_approx_output_size = ApproxTopKReductionOutputSize(
      n, rank, top_k, recall_target, /*aggregate_to_topk=*/false,
      reduction_input_size_override);
  if (!status_or_approx_output_size.status().ok()) {
    return builder->ReportError(status_or_approx_output_size.status());
  }

  int64_t approx_output_size, log2_reduction;
  std::tie(approx_output_size, log2_reduction) =
      status_or_approx_output_size.ValueOrDie();

  if (log2_reduction == 0) {
    if (aggregate_to_topk) {
      return SortAndSliceBuilder(builder, operands, top_k, reduction_dim,
                                 comparator);
    }
    return Tuple(builder, operands);
  }

  std::vector<XlaOp> partial_reduce_args;
  partial_reduce_args.reserve(operands.size() + init_values.size());
  for (const auto& op : operands) {
    partial_reduce_args.push_back(op);
  }
  for (const auto& op : init_values) {
    partial_reduce_args.push_back(op);
  }
  std::vector<Shape> approx_output_shapes;
  approx_output_shapes.reserve(operands_shapes.size());
  for (auto op_shape : operands_shapes) {
    op_shape.mutable_dimensions()[reduction_dim] = approx_output_size;
    approx_output_shapes.push_back(op_shape);
  }
  auto approx_output_shape = ShapeUtil::MakeTupleShape(approx_output_shapes);
  // PartialReduce option in JSON form
  std::string partial_reduce_option = absl::StrFormat(
      "{\"log2_reduction\": %d, \"reduction_dim\": %d, \"to_apply_type\": "
      "\"comparator\"}",
      log2_reduction, reduction_dim);

  auto approx_topk = CustomCallWithComputation(
      builder, "PartialReduce", partial_reduce_args, comparator,
      approx_output_shape, partial_reduce_option);

  if (aggregate_to_topk) {
    std::vector<XlaOp> approx_topk_results;
    approx_topk_results.reserve(num_operands);
    for (int i = 0; i < num_operands; ++i) {
      approx_topk_results.push_back(GetTupleElement(approx_topk, i));
    }
    return SortAndSliceBuilder(builder, approx_topk_results, top_k,
                               reduction_dim, comparator);
  }
  return approx_topk;
}

inline uint32_t log2_floor(uint64_t value) {
  return value == 0 ? 0 : tensorflow::Log2Floor64(value);
}

inline uint32_t log2_ceil(uint64_t value) {
  return value == 0 ? 0 : tensorflow::Log2Ceiling64(value);
}

StatusOr<std::pair<int64_t, int64_t>> ApproxTopKReductionOutputSize(
    int64_t input_size, int64_t rank, int64_t top_k, float recall_target,
    bool aggregate_to_topk, int64_t input_size_override) {
  // Fallback to variadic reduce when top_k == 1.
  // TODO(fchern): Approx-topk followed by variadic reduce might run faster
  // than running variadic reduce directly.
  if (top_k == 1) {
    return std::pair<int64_t, int64_t>(1, -1);
  }

  if (aggregate_to_topk) {
    return std::pair<int64_t, int64_t>(top_k, -1);
  }

  uint64_t tpu_tiling = rank == 1 ? kTpuChunkTiling : kTpuLaneTiling;
  if (input_size <= tpu_tiling) {
    return std::pair<int64_t, int64_t>(input_size, 0);
  }

  if (recall_target <= 0. || recall_target > 1.0) {
    return InvalidArgument("recall_target should range in (0,1]");
  }

  // Need to handle 1.0 explicitly, otherwise we would encounter division by
  // log(1.0) = 0 issue.
  if (recall_target == 1.0) {
    return std::pair<int64_t, int64_t>(input_size, 0);
  }

  if (input_size_override >= 0) {
    if (input_size > input_size_override) {
      return InvalidArgument(
          "reduction_input_size_override: %d should be greater "
          "equals to operands[reduction_dim]: %d",
          input_size_override, input_size);
    }
  }
  uint64_t logical_input_size =
      input_size_override >= 0 ? input_size_override : input_size;

  // Given number of data points N, K for top-k elements, and W for the size of
  // the reduce window, let M = Ceil(N / W) be the number of windows. The
  // expected number of top-k elements that doesn't collide in windows is
  //
  //   K * ((M - 1) / M)^{K - 1}
  //
  // The recall of is the expected number of top-k elements divided by K
  //
  //   recall = ((M - 1) / M)^{K - 1}
  //          = (1 - 1/M)^{K - 1}
  //          = (1 - 1/M)^{-M * (K - 1)/(-M)}
  //          ~= EXP((1 - K) / M)    for large M
  //
  //   => M = (1 - K)/LOG(recall)
  uint64_t m = std::min<uint64_t>(
      std::max(
          static_cast<uint64_t>((1.0 - top_k) /
                                std::log(static_cast<double>(recall_target))),
          tpu_tiling),
      input_size);
  uint32_t log2_reduction = log2_floor(logical_input_size / m);
  if (log2_reduction == 0) {
    return std::pair<int64_t, int64_t>(input_size, 0);
  }

  // Do not reduce too much when logical_input is too large.
  log2_reduction =
      std::min<uint32_t>(log2_reduction, log2_ceil(input_size / tpu_tiling));

  int64_t approx_output_size =
      CeilOfRatio<int64_t>(CeilOfRatio<int64_t>(input_size, tpu_tiling),
                           (1 << log2_reduction)) *
      tpu_tiling;

  return std::pair<int64_t, int64_t>(approx_output_size, log2_reduction);
}

}  // namespace xla
