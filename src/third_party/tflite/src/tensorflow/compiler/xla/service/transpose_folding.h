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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_TRANSPOSE_FOLDING_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_TRANSPOSE_FOLDING_H_

#include <functional>

#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"

namespace xla {

// HLO pass that folds transpose operators into Dot operators, where the Dot
// operator is implemented by a GEMM kernel that can transpose its inputs.
class TransposeFolding : public HloModulePass {
 public:
  using OperandIndices = std::vector<int64_t>;

  // Returns the set of foldable operands for a given HLO and some candidate
  // operands.
  using TransposableConvOperandsFn = std::function<OperandIndices(
      const HloInstruction&, const OperandIndices&)>;

  using CanFoldTransposeOperand = std::function<StatusOr<bool>(
      const HloInstruction&, int64_t /*operand_idx*/)>;

  // Helper function to explicitly not fold transposes.
  static OperandIndices NeverFoldTranspose(const HloInstruction&,
                                           const OperandIndices&) {
    return {};
  }

  // Helper function to always fold transposes.
  static OperandIndices AlwaysFoldTranspose(const HloInstruction&,
                                            const OperandIndices& ids) {
    return ids;
  }

  // `dot_can_fold_transpose_operand` returns whether the dot operation can fold
  // in the given transpose operand.
  //
  // transposable_conv_operands returns the set of operands it wants to fold if
  // the instruction argument is implemented as a convolution that supports
  // transposing its arguments.
  explicit TransposeFolding(
      CanFoldTransposeOperand dot_can_fold_transpose_operand =
          IsRowColumnTransposeDotOperand,
      TransposableConvOperandsFn transposable_conv_operands =
          AlwaysFoldTranspose);
  absl::string_view name() const override { return "transpose-folding"; }

  StatusOr<bool> Run(HloModule* module) override;

  static StatusOr<bool> IsRowColumnTransposeDotOperand(
      const HloInstruction& dot, int64_t operand_idx);

 private:
  CanFoldTransposeOperand dot_can_fold_transpose_operand_;
  TransposableConvOperandsFn transposable_conv_operands_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_TRANSPOSE_FOLDING_H_
