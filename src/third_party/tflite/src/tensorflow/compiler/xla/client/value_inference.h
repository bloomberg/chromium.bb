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
#ifndef TENSORFLOW_COMPILER_XLA_CLIENT_VALUE_INFERENCE_H_
#define TENSORFLOW_COMPILER_XLA_CLIENT_VALUE_INFERENCE_H_

#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/dfs_hlo_visitor.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_evaluator.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {
// OptionalLiteral is an augmented literal class which returns optional
// values for each index (the value can be either valid or invalid). The
// implementation keeps two literals, a value literal, holding both the valid
// and garabage value, and a masking literal representing if a value is valid or
// garbage.
class OptionalLiteral {
 public:
  explicit OptionalLiteral(Literal value, Literal mask)
      : value_(std::move(value)), mask_(std::move(mask)) {}

  template <typename NativeT>
  absl::optional<NativeT> Get(absl::Span<const int64_t> element_index,
                              ShapeIndex shape_index = {}) const {
    if (mask_.Get<bool>(element_index, shape_index)) {
      return absl::nullopt;
    } else {
      return value_.Get<NativeT>(element_index, shape_index);
    }
  }

  // Returns true if all values in this literal slice are value.
  bool AllValid() { return mask_.IsAll(0); }

  // Get value out of this slice if all values are valid. Otherwise returns
  // nullopt.
  absl::optional<LiteralSlice> GetValue() {
    if (!AllValid()) {
      return absl::nullopt;
    }
    return LiteralSlice(value_);
  }

 private:
  Literal value_;
  Literal mask_;
};

enum ValueInferenceMode {
  // Inference the constant value itself.
  kValue = 0,
  // Inference upper-bound and lower-bound of the value. Bounds are inclusive.
  kUpperBound,
  kLowerBound,
};

class ValueInference {
 public:
  // ValueInference analyzes values in XlaOp answers following questions:
  // - What's the upper-bound of each value in a tensor.
  // - What's the lower-bound of each value in a tensor.
  // - What's the constant value of each tensor.
  // - Whether or not each value in a tensor is dynamic.
  explicit ValueInference(XlaBuilder* builder) : builder_(builder) {
    CHECK(builder_);
  }
  StatusOr<Literal> AnalyzeIsDynamic(XlaOp op);
  // Returns an OptionalLiteral. Each individual value of the literal is
  // the concrete constant value if it can be inferred, otherwise a nullopt.
  StatusOr<OptionalLiteral> AnalyzeConstant(XlaOp op, ValueInferenceMode mode);

  // Returns underlying xla builder.
  XlaBuilder* builder() { return builder_; }

 private:
  // Given an op handle, returns a simplified version of the handle inside a
  // int64_t Literal. If the a -1 value for the handle means invalid
  // simplification and the result shouldn't be used.
  StatusOr<Literal> SimplifyOp(int64_t handle);

  // Perform CSE on a given handle, and return an equivalent handle if seen
  // before. Otherwise, returns nullopt.
  StatusOr<absl::optional<int64_t>> CseOpHandle(int64_t handle);
  XlaBuilder* builder_;
  HloEvaluator evaluator_;
  // A map from instruction_hash to handle that helps perform CSE.
  absl::flat_hash_map<int64_t, int64_t> cse_map_;
};
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_CLIENT_VALUE_INFERENCE_H_
