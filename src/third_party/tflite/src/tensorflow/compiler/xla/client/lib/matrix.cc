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

#include "tensorflow/compiler/xla/client/lib/matrix.h"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "tensorflow/compiler/xla/client/lib/arithmetic.h"
#include "tensorflow/compiler/xla/client/lib/constants.h"
#include "tensorflow/compiler/xla/client/lib/slicing.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {

XlaOp IdentityMatrix(XlaBuilder* builder, PrimitiveType type, int64 m,
                     int64 n) {
  auto a = Iota(builder, U32, m);
  auto b = Iota(builder, U32, n);
  auto indicator = Eq(a, Broadcast(b, {m}), /*broadcast_dimensions=*/{0});
  return ConvertElementType(indicator, type);
}

XlaOp GetDiagonalMask(XlaOp x, int diagonal) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(x));
    auto n_dims = static_cast<int32>(shape.rank());
    TF_RET_CHECK(n_dims >= 2);
    auto m = shape.dimensions(n_dims - 2);
    auto n = shape.dimensions(n_dims - 1);
    absl::Span<const int64> major_dims =
        AsInt64Slice(shape.dimensions()).subspan(/*pos=*/0, /*len=*/n_dims - 2);
    auto a = Iota(builder, S32, n);
    auto b = Iota(builder, S32, m) + ConstantR0WithType(builder, S32, diagonal);
    auto indicator = Eq(b, Broadcast(a, {m}), /*broadcast_dimensions=*/{0});
    auto mask = Broadcast(indicator, major_dims);
    return mask;
  });
}

XlaOp GetMatrixDiagonal(XlaOp x, int k) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(x));
    auto n_dims = static_cast<int32>(shape.rank());
    TF_RET_CHECK(n_dims >= 2);
    const int64 m = shape.dimensions(n_dims - 2);
    const int64 n = shape.dimensions(n_dims - 1);

    if (k <= -m || k >= n) {
      auto zero_size_shape = shape;
      zero_size_shape.DeleteDimension(n_dims - 1);
      zero_size_shape.set_dimensions(n_dims - 2, 0);
      return ConstantLiteral(builder, Literal{zero_size_shape});
    }
    auto mask = GetDiagonalMask(x, k);

    int64 reduce_dim = n_dims - 1;
    if ((k == 0 && m >= n) || k < 0) {
      reduce_dim = n_dims - 2;
    }
    auto result = Reduce(
        Select(mask, x, Zeros(builder, shape)), ScalarLike(x, 0),
        CreateScalarIdentityWithZeroComputation(shape.element_type(), builder),
        {reduce_dim});
    // k == 0, we can save one slice op.
    if (k == 0) {
      return result;
    }
    return SliceInMinorDims(result, {0},
                            {k > 0 ? std::min(m, n - k) : std::min(n, m + k)});
  });
}

XlaOp GetMatrixDiagonalViaGather(XlaOp x, int k) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(x));
    auto n_dims = static_cast<int32>(shape.rank());
    TF_RET_CHECK(n_dims >= 2);
    const int64 m = shape.dimensions(n_dims - 2);
    const int64 n = shape.dimensions(n_dims - 1);

    // The start_indices has a shape of {diag_len, 2}, and each pair of value in
    // its dimension 1 represents the (row, col) of the diagonal. We set
    // index_vector_dim to 1 and make start_index_map and collapsed_slice_dims
    // contain the same two dimension indices. This makes sure that the (row,
    // col) pairs in start_indices are propagated to the indices for the two
    // collapsed dimensions in the operand indices through start_index_map.
    const int64 num_index_dims = 2;
    const int64 axis = n_dims - num_index_dims;

    // Calculate the indices of diagonal part with offset k.
    const int64 diag_len =
        std::max(std::min(m + std::min(k, 0), n - std::max(k, 0)), int64{0});
    XlaOp diag_base_indices = BroadcastInDim(Iota(builder, S32, diag_len),
                                             {diag_len, num_index_dims}, {0});
    XlaOp diag_offset =
        Broadcast(ConstantR1<int>(builder, {std::max(-k, 0), std::max(k, 0)}),
                  {diag_len});
    XlaOp start_indices = Add(diag_base_indices, diag_offset);

    // Example of a 3D diag-part extracting diagonal part with offset=1 out of a
    // tensor of shape [2,5,4].
    //
    //  operand = s32[2,5,4] parameter(0)
    //  indices = s32[3,2] parameter(1)
    //  gather = s32[2,3] gather(operand, indices),
    //       offset_dims={0},
    //       collapsed_slice_dims={1,2},
    //       start_index_map={1,2},
    //       index_vector_dim=1,
    //       slice_sizes={2, 1, 1}

    xla::GatherDimensionNumbers dim_numbers;
    std::vector<int64> slice_sizes;
    slice_sizes.reserve(n_dims);
    for (int64 i = 0; i < n_dims; i++) {
      int64 window_bound;
      if (axis <= i) {
        dim_numbers.add_collapsed_slice_dims(i);
        dim_numbers.add_start_index_map(i);
        window_bound = (shape.dimensions(i) != 0) ? 1 : 0;
      } else {
        dim_numbers.add_offset_dims(i);
        window_bound = shape.dimensions(i);
      }
      slice_sizes.push_back(window_bound);
    }

    dim_numbers.set_index_vector_dim(1);

    return Gather(x, start_indices, dim_numbers, slice_sizes,
                  /*indices_are_sorted=*/true);
  });
}

XlaOp SetMatrixDiagonal(XlaOp matrix, XlaOp diag, int k) {
  XlaBuilder* builder = matrix.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(matrix));
    TF_ASSIGN_OR_RETURN(Shape diag_shape, builder->GetShape(diag));
    auto n_dims = static_cast<int32>(shape.rank());
    TF_RET_CHECK(n_dims >= 2);
    const int64 m = shape.dimensions(n_dims - 2);
    const int64 n = shape.dimensions(n_dims - 1);
    const int64 d = diag_shape.dimensions(n_dims - 2);
    std::vector<int64> broadcast_dims(n_dims - 1);
    absl::c_iota(broadcast_dims, 0);
    int64 pad_high = m - d;
    if (k < 0) {
      ++(broadcast_dims.back());
      pad_high = n - d;
    }

    if (pad_high != 0) {
      PaddingConfig padding_config;
      for (xla::int64 i = 0; i < diag_shape.rank() - 1; ++i) {
        auto* dims = padding_config.add_dimensions();
        dims->set_edge_padding_low(0);
        dims->set_interior_padding(0);
        dims->set_edge_padding_high(0);
      }
      auto* dims = padding_config.add_dimensions();
      dims->set_edge_padding_low(0);
      dims->set_interior_padding(0);
      dims->set_edge_padding_high(pad_high);
      diag = Pad(diag, ScalarLike(diag, 0), padding_config);
    }

    return Select(GetDiagonalMask(matrix, k),
                  BroadcastInDim(diag, shape.dimensions(), broadcast_dims),
                  matrix);
  });
}

XlaOp TriangleMask(XlaOp x, int diagonal) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(x));
    const int64 n_dims = shape.rank();
    TF_RET_CHECK(n_dims >= 2);
    const int64 m = shape.dimensions(n_dims - 2);
    const int64 n = shape.dimensions(n_dims - 1);
    absl::Span<const int64> major_dims =
        AsInt64Slice(shape.dimensions()).subspan(/*pos=*/0, /*len=*/n_dims - 2);
    auto a = Iota(builder, S32, n);
    auto b = Iota(builder, S32, m) + ConstantR0<int32>(builder, diagonal);
    XlaOp indicator;
    indicator = Ge(b, Broadcast(a, {m}), /*broadcast_dimensions=*/{0});
    return Broadcast(indicator, major_dims);
  });
}

XlaOp Triangle(XlaOp x, bool lower) {
  return lower ? Select(TriangleMask(x, 0), x, ZerosLike(x))
               : Select(TriangleMask(x, -1), ZerosLike(x), x);
}

XlaOp UpperTriangle(XlaOp x) { return Triangle(x, false); }

XlaOp LowerTriangle(XlaOp x) { return Triangle(x, true); }

namespace {
std::vector<int64> EinsumDiagonalLabels(absl::Span<const int64> config) {
  std::vector<int64> unique_labels;
  for (auto label = config.begin(); label != config.end(); ++label) {
    auto first_label = absl::c_find(config, *label);
    if (first_label == label) {
      unique_labels.push_back(*label);
    }
  }
  if (unique_labels.size() == config.size()) {
    unique_labels.clear();
  }
  return unique_labels;
}
}  // namespace

xla::XlaOp EinsumDiagonal(XlaOp x, absl::Span<const int64> config) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (EinsumDiagonalLabels(config).empty()) {
      return x;
    }
    TF_ASSIGN_OR_RETURN(Shape x_shape, builder->GetShape(x));
    Shape iota_shape = x_shape;
    iota_shape.set_element_type(S32);
    XlaOp mask = ConstantR0(builder, true);

    absl::InlinedVector<int64, 8> reduce_dims;
    for (auto label = config.begin(); label != config.end(); ++label) {
      const int64 dim = label - config.begin();
      auto first_label = absl::c_find(config, *label);
      if (first_label == label) {
        continue;
      }
      reduce_dims.push_back(dim);
      const int64 first_dim = first_label - config.begin();
      mask = And(mask, Eq(Iota(builder, iota_shape, first_dim),
                          Iota(builder, iota_shape, dim)));
    }
    auto zero = ScalarLike(x, 0);
    return Reduce(Select(mask, x, zero), zero,
                  CreateScalarIdentityWithZeroComputation(
                      x_shape.element_type(), builder),
                  reduce_dims);
  });
}

Status ValidateEinsumNumericDimensions(absl::Span<const int64> x_config,
                                       absl::Span<const int64> y_config,
                                       absl::Span<const int64> output_config) {
  for (auto dim : output_config) {
    if (absl::c_linear_search(x_config, dim) ||
        absl::c_linear_search(y_config, dim)) {
      if (absl::c_count(output_config, dim) > 1) {
        return InvalidArgument("Einsum has repeated output dimension.");
      }
      continue;
    }
    return InvalidArgument(
        "Einsum has output dimension without corresponding input dimension.");
  }
  for (auto dim : x_config) {
    if (absl::c_linear_search(y_config, dim) ||
        absl::c_linear_search(output_config, dim)) {
      if (absl::c_count(x_config, dim) > 1) {
        return InvalidArgument("Einsum has repeated lhs dimension.");
      }
    }
  }
  for (auto dim : y_config) {
    if (absl::c_linear_search(x_config, dim) ||
        absl::c_linear_search(output_config, dim)) {
      if (absl::c_count(y_config, dim) > 1) {
        return InvalidArgument("Einsum has repeated rhs dimension.");
      }
    }
  }
  return Status::OK();
}

namespace {
// Helper method to remove dimensions from a shape and dot dimension numbers
// used to implement implicit broadcasting.
template <typename C>
void DeleteDimsFromContainer(absl::Span<const int64> to_delete, Shape* shape,
                             C* batch_dims, C* contracting_dims) {
  if (to_delete.empty()) {
    return;
  }
  for (int64 i = to_delete.size() - 1; i >= 0; --i) {
    int64 dim = to_delete[i];
    shape->DeleteDimension(dim);
    for (auto& b : *batch_dims) {
      if (b > dim) {
        --b;
      }
    }
    for (auto& c : *contracting_dims) {
      if (c > dim) {
        --c;
      }
    }
  }
}
}  // namespace

xla::XlaOp Einsum(xla::XlaOp x, absl::Span<const int64> x_config, xla::XlaOp y,
                  absl::Span<const int64> y_config,
                  absl::Span<const int64> output_config,
                  xla::PrecisionConfig::Precision precision) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    auto x_diagonal_labels = EinsumDiagonalLabels(x_config);
    auto y_diagonal_labels = EinsumDiagonalLabels(y_config);
    if (!x_diagonal_labels.empty() && !y_diagonal_labels.empty()) {
      return Einsum(EinsumDiagonal(x, x_config), x_diagonal_labels,
                    EinsumDiagonal(y, y_config), y_diagonal_labels,
                    output_config, precision);
    } else if (!x_diagonal_labels.empty()) {
      return Einsum(EinsumDiagonal(x, x_config), x_diagonal_labels, y, y_config,
                    output_config, precision);
    } else if (!y_diagonal_labels.empty()) {
      return Einsum(x, x_config, EinsumDiagonal(y, y_config), y_diagonal_labels,
                    output_config, precision);
    }

    TF_RETURN_IF_ERROR(
        ValidateEinsumNumericDimensions(x_config, y_config, output_config));
    TF_ASSIGN_OR_RETURN(Shape x_shape, builder->GetShape(x));
    TF_ASSIGN_OR_RETURN(Shape y_shape, builder->GetShape(y));
    const int64 x_rank = x_config.size();
    const int64 y_rank = y_config.size();
    const int64 output_rank = output_config.size();
    absl::flat_hash_set<int64> x_map;
    absl::flat_hash_set<int64> y_map;
    absl::flat_hash_set<int64> output_map;

    for (auto d : x_config) {
      if (!x_map.insert(d).second) {
        return InvalidArgument("XLA Einsum does not support rhs tracing");
      }
    }

    for (auto d : y_config) {
      if (!y_map.insert(d).second) {
        return InvalidArgument("XLA Einsum does not support lhs tracing");
      }
    }

    for (auto d : output_config) {
      if (!output_map.insert(d).second) {
        return InvalidArgument("XLA Einsum does not support output tracing");
      }
    }

    DotDimensionNumbers dnums;
    std::vector<int64> lhs_outer_dims;
    auto is_batch_dim = [&](int64 d) {
      return x_map.contains(d) && y_map.contains(d) && output_map.contains(d);
    };
    auto is_contracting = [&](int64 d) {
      return x_map.contains(d) && y_map.contains(d);
    };
    auto rhs_dimension_number = [&](int64 d) {
      return absl::c_find(y_config, d) - y_config.begin();
    };

    absl::InlinedVector<int64, 8> rhs_outer_dims;
    absl::InlinedVector<int64, 8> rhs_delete_dims;
    absl::InlinedVector<int64, 8> lhs_delete_dims;
    for (int64 i = 0; i < x_rank; ++i) {
      auto dim_name = x_config[i];
      const int64 rhs_dim = rhs_dimension_number(dim_name);
      if (is_batch_dim(dim_name)) {
        if (x_shape.dimensions(i) == y_shape.dimensions(rhs_dim)) {
          dnums.add_lhs_batch_dimensions(i);
          dnums.add_rhs_batch_dimensions(rhs_dim);
        } else if (x_shape.dimensions(i) == 1) {
          rhs_outer_dims.push_back(rhs_dim);
          lhs_delete_dims.push_back(i);
        } else {
          lhs_outer_dims.push_back(i);
          rhs_delete_dims.push_back(rhs_dim);
        }
      } else if (is_contracting(dim_name)) {
        if (x_shape.dimensions(i) == y_shape.dimensions(rhs_dim)) {
          dnums.add_lhs_contracting_dimensions(i);
          dnums.add_rhs_contracting_dimensions(rhs_dim);
        } else if (x_shape.dimensions(i) == 1) {
          rhs_outer_dims.push_back(rhs_dim);
          lhs_delete_dims.push_back(i);
        } else {
          lhs_outer_dims.push_back(i);
          rhs_delete_dims.push_back(rhs_dim);
        }
      } else {
        lhs_outer_dims.push_back(i);
      }
    }

    for (int64 i = 0; i < y_rank; ++i) {
      auto dim_name = y_config[i];
      if (!is_batch_dim(dim_name) && !is_contracting(dim_name)) {
        rhs_outer_dims.push_back(i);
      }
    }

    absl::c_sort(rhs_outer_dims);

    absl::InlinedVector<int64, 8> output_transpose_dims;
    absl::InlinedVector<int64, 8> output_reduce_dims;
    auto output_dimension_number = [&](int64 d) {
      auto pos = absl::c_find(output_config, d);
      if (pos == output_config.end()) {
        const int64 dim =
            output_transpose_dims.size() + output_reduce_dims.size();
        output_reduce_dims.push_back(dim);
      } else {
        output_transpose_dims.push_back(pos - output_config.begin());
      }
    };

    for (auto d : dnums.lhs_batch_dimensions()) {
      output_dimension_number(x_config[d]);
    }

    for (auto d : lhs_outer_dims) {
      output_dimension_number(x_config[d]);
    }

    for (auto d : rhs_outer_dims) {
      output_dimension_number(y_config[d]);
    }

    std::vector<int64> transpose_dims(output_rank);
    for (int64 i = 0; i < output_rank; ++i) {
      transpose_dims[output_transpose_dims[i]] = i;
    }

    // Remove ones that where broadcasted from the x and the y shape and adjust
    // the dimension numbers that are more minor than those dimensions.
    DeleteDimsFromContainer(lhs_delete_dims, &x_shape,
                            dnums.mutable_lhs_batch_dimensions(),
                            dnums.mutable_lhs_contracting_dimensions());
    DeleteDimsFromContainer(rhs_delete_dims, &y_shape,
                            dnums.mutable_rhs_batch_dimensions(),
                            dnums.mutable_rhs_contracting_dimensions());
    if (!lhs_delete_dims.empty()) {
      x = Reshape(x, x_shape.dimensions());
    }

    if (!rhs_delete_dims.empty()) {
      y = Reshape(y, y_shape.dimensions());
    }

    PrecisionConfig precision_proto;
    precision_proto.add_operand_precision(precision);
    precision_proto.add_operand_precision(precision);
    auto dot = DotGeneral(x, y, dnums, &precision_proto);
    if (!output_reduce_dims.empty()) {
      dot = Reduce(dot, ScalarLike(dot, 0),
                   CreateScalarAddComputation(x_shape.element_type(), builder),
                   output_reduce_dims);
    }
    return Transpose(dot, transpose_dims);
  });
}

XlaOp BatchDot(XlaOp x, XlaOp y, PrecisionConfig::Precision precision) {
  return BatchDot(x, false, y, false, precision);
}

XlaOp BatchDot(XlaOp x, bool transpose_x, XlaOp y, bool transpose_y,
               PrecisionConfig::Precision precision) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    std::string string("...mk,...kn->...mn");
    if (transpose_x) {
      std::swap(string[3], string[4]);
    }
    if (transpose_y) {
      std::swap(string[6 + 3], string[6 + 4]);
    }
    return Einsum(x, y, string, precision);
  });
}

StatusOr<std::array<std::vector<int64>, 3>> ParseEinsumString(
    absl::string_view einsum_config, int64 x_rank, int64 y_rank) {
  std::array<std::vector<int64>, 3> einsum_config_numeric;
  std::vector<absl::string_view> main_split =
      absl::StrSplit(einsum_config, ',');
  if (main_split.size() != 2) {
    return InvalidArgument("Expected one \",\" in einsum_config.");
  }

  auto maybe_invalid_character = [](char d) {
    if (absl::ascii_isalpha(d)) {
      return Status::OK();
    }
    if (d == '.') {
      return InvalidArgument("Unsupported \".\" in einsum config.");
    }
    return InvalidArgument("Unexpected character in einsum config.");
  };

  auto string_config_to_numeric =
      [&](absl::string_view config, bool is_input_config, int64 input_rank,
          int64 ellipsis_rank,
          std::vector<int64>* numeric_config) -> StatusOr<int64> {
    std::vector<absl::string_view> splits = absl::StrSplit(config, "...");
    if (splits.empty()) {
      return ellipsis_rank;
    }
    if (splits.size() > 2) {
      return InvalidArgument("Too many ellipses (\"...\") in einsum config.");
    }
    // There is one split if we don't have an ellipsis, and two splits if we do.
    const bool has_ellipsis = splits.size() > 1;
    // We only compute ellipsis_rank for input configs.
    if (is_input_config && has_ellipsis) {
      // ellipsis_rank is input rank minus the number of named labels.
      ellipsis_rank =
          input_rank - static_cast<int64>(splits[0].size() + splits[1].size());
      if (ellipsis_rank < 0) {
        return InvalidArgument(
            "Too few dimensions in the input for the given einsum config.");
      }
    }
    for (char d : splits[0]) {
      TF_RETURN_IF_ERROR(maybe_invalid_character(d));
      numeric_config->push_back(static_cast<int64>(d));
    }
    if (has_ellipsis) {
      // For input configs, we use the value of ellipsis_rank we just computed.
      // For output config, we use the existing value of ellipsis_rank.
      for (int64 i = ellipsis_rank; i > 0; --i) {
        numeric_config->push_back(-i);
      }
      for (char d : splits[1]) {
        TF_RETURN_IF_ERROR(maybe_invalid_character(d));
        numeric_config->push_back(static_cast<int64>(d));
      }
    }
    return ellipsis_rank;
  };

  TF_ASSIGN_OR_RETURN(
      const int64 x_ellipsis_rank,
      string_config_to_numeric(main_split[0],
                               /*is_input_config=*/true, x_rank,
                               /*ellipsis_rank=*/0, &einsum_config_numeric[0]));

  std::vector<absl::string_view> y_output_split =
      absl::StrSplit(main_split[1], "->");
  if (y_output_split.size() != 2) {
    return InvalidArgument("Expected one \"->\" in einsum_config.");
  }

  TF_ASSIGN_OR_RETURN(
      const int64 y_ellipsis_rank,
      string_config_to_numeric(y_output_split[0],
                               /*is_input_config=*/true, y_rank,
                               /*ellipsis_rank=*/0, &einsum_config_numeric[1]));

  // Replace ellipsis in output_config with numeric labels with the same
  // ellipsis rank as in the inputs.
  // Note: This implementation doesn't support different-rank broadcasting.
  TF_ASSIGN_OR_RETURN(
      std::ignore,
      string_config_to_numeric(
          y_output_split[1], /*is_input_config=*/false,
          /*input_rank=*/0,
          /*ellipsis_rank=*/std::max(x_ellipsis_rank, y_ellipsis_rank),
          &einsum_config_numeric[2]));
  return einsum_config_numeric;
}

std::string NormalizeEinsumString(absl::string_view einsum_config) {
  if (einsum_config.find("->") != einsum_config.npos) {
    return "";
  }
  bool has_ellipsis = einsum_config.find("...") != einsum_config.npos;
  std::map<char, int64> chars;
  for (char c : einsum_config) {
    if (absl::ascii_isalpha(c)) {
      ++chars[c];
    }
  }
  std::string new_config(einsum_config.begin(), einsum_config.end());
  new_config.append("->");
  if (has_ellipsis) {
    new_config.append("...");
  }
  for (auto p : chars) {
    if (p.second == 1) {
      new_config.push_back(p.first);
    }
  }
  return new_config;
}

XlaOp Einsum(XlaOp x, XlaOp y, absl::string_view einsum_config,
             PrecisionConfig::Precision precision) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    auto new_config = NormalizeEinsumString(einsum_config);
    if (!new_config.empty()) {
      return Einsum(x, y, new_config, precision);
    }
    TF_ASSIGN_OR_RETURN(Shape x_shape, builder->GetShape(x));
    TF_ASSIGN_OR_RETURN(Shape y_shape, builder->GetShape(y));
    TF_ASSIGN_OR_RETURN(
        auto einsum_config_numeric,
        ParseEinsumString(einsum_config, x_shape.rank(), y_shape.rank()));
    return Einsum(x, einsum_config_numeric[0], y, einsum_config_numeric[1],
                  einsum_config_numeric[2], precision);
  });
}

XlaOp Einsum(XlaOp x, absl::string_view einsum_config,
             PrecisionConfig::Precision precision) {
  return Einsum(ScalarLike(x, 1), x, absl::StrCat(",", einsum_config),
                precision);
}

XlaOp TransposeInMinorDims(XlaOp x) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, builder->GetShape(x));
    const int64 n_dims = shape.rank();
    TF_RET_CHECK(n_dims >= 2);
    std::vector<int64> permutation(n_dims);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::swap(permutation[n_dims - 1], permutation[n_dims - 2]);
    return Transpose(x, permutation);
  });
}

XlaOp MaybeTransposeInMinorDims(XlaOp x, bool transpose) {
  return transpose ? TransposeInMinorDims(x) : x;
}

}  // namespace xla
