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

#include "tensorflow/compiler/xla/service/gpu/cublas_gemm_pad_for_tensor_cores.h"

#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/gpu/ir_emission_utils.h"
#include "tensorflow/compiler/xla/service/hlo_casting_utils.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/window_util.h"
#include "tensorflow/core/platform/types.h"

namespace xla {
namespace gpu {

static StatusOr<bool> PadForTensorCores(HloDotInstruction* dot) {
  auto* lhs = dot->mutable_operand(0);
  auto* rhs = dot->mutable_operand(1);

  Shape lshape = lhs->shape();
  Shape rshape = rhs->shape();
  Shape result_shape = dot->shape();

  if (lshape.element_type() != PrimitiveType::F16 ||
      rshape.element_type() != PrimitiveType::F16) {
    return false;
  }

  auto pad_dim = [](Shape& s, int64 dim) {
    s.set_dimensions(dim, RoundUpToNearest<int64>(s.dimensions(dim), 8));
  };

  auto pad_matrix_dims = [&pad_dim](Shape s) {
    // Since the dot instruction is canonicalized, the last two dimensions for
    // each operand represent non-batch dimensions, and the others are the same
    // for both operands and correspond to batch dimensions.
    pad_dim(s, s.rank() - 2);
    pad_dim(s, s.rank() - 1);
    return s;
  };

  Shape new_lshape = pad_matrix_dims(lshape);
  Shape new_rshape = pad_matrix_dims(rshape);
  Shape new_result_shape = pad_matrix_dims(result_shape);

  if (new_lshape == lshape && new_rshape == rshape) {
    return false;
  }

  VLOG(3) << "old shape: " << lshape << " " << rshape << " " << result_shape;
  VLOG(3) << "new shape: " << new_lshape << " " << new_rshape << " "
          << new_result_shape;

  auto create_padding_config = [](Shape& shape, Shape& new_shape) {
    PaddingConfig padding_config;
    for (int i = 0; i < shape.rank(); ++i) {
      auto dimension = padding_config.add_dimensions();
      dimension->set_edge_padding_high(new_shape.dimensions()[i] -
                                       shape.dimensions()[i]);
      dimension->set_edge_padding_low(0);
      dimension->set_interior_padding(0);
    }
    return padding_config;
  };

  auto l_padding_config = create_padding_config(lshape, new_lshape);
  auto r_padding_config = create_padding_config(rshape, new_rshape);

  HloComputation* parent = dot->parent();

  HloInstruction* zero_float = parent->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<half>((half)0.0)));
  zero_float->set_metadata(dot->metadata());

  HloInstruction* lpad = parent->AddInstruction(
      HloInstruction::CreatePad(new_lshape, lhs, zero_float, l_padding_config));
  lpad->set_metadata(dot->metadata());

  HloInstruction* rpad = parent->AddInstruction(
      HloInstruction::CreatePad(new_rshape, rhs, zero_float, r_padding_config));
  rpad->set_metadata(dot->metadata());

  HloInstruction* new_dot = parent->AddInstruction(
      dot->CloneWithNewOperands(new_result_shape, {lpad, rpad}));

  std::vector<int64> start_indices(result_shape.rank(), 0);
  std::vector<int64> strides(result_shape.rank(), 1);
  HloInstruction* slice = parent->AddInstruction(
      HloInstruction::CreateSlice(result_shape, new_dot, start_indices,
                                  result_shape.dimensions(), strides));
  slice->set_metadata(dot->metadata());

  bool is_root = dot->user_count() == 0;

  TF_CHECK_OK(parent->ReplaceInstruction(dot, slice));

  if (is_root) {
    parent->set_root_instruction(slice);
  }

  return true;
}

namespace {

// We need this check because PadForTensorCores works in the assumption that
// the dot instruction is canonicalized.
bool CheckCanonical(HloDotInstruction* dot) {
  auto dimension_numbers = dot->dot_dimension_numbers();

  if (dimension_numbers.lhs_batch_dimensions_size() + 2 !=
          dot->operand(0)->shape().rank() ||
      dimension_numbers.rhs_batch_dimensions_size() + 2 !=
          dot->operand(1)->shape().rank()) {
    LOG(ERROR) << "Dot is not canonical: Expected all dimensions but 2 to be "
                  "batch_dimensions.";
    return false;
  }

  std::vector<int64> canonical_batch_dims(
      dimension_numbers.lhs_batch_dimensions_size());
  absl::c_iota(canonical_batch_dims, 0);
  if (!absl::c_equal(dimension_numbers.lhs_batch_dimensions(),
                     canonical_batch_dims) ||
      !absl::c_equal(dimension_numbers.rhs_batch_dimensions(),
                     canonical_batch_dims)) {
    LOG(ERROR) << "Dot is not canonical: Expected batch dimensions to be all "
                  "dimensions except for the last 2 ones.";
    return false;
  }

  return true;
}

}  // namespace

static std::vector<HloDotInstruction*> GetRelevantDots(HloComputation* comp) {
  std::vector<HloDotInstruction*> convs;

  for (HloInstruction* instr : comp->instructions()) {
    if (IsMatrixMultiplication(*instr)) {
      HloDotInstruction* dot = Cast<HloDotInstruction>(instr);
      if (CheckCanonical(dot)) {
        convs.push_back(dot);
      }
    }
  }
  return convs;
}

StatusOr<bool> CublasGemmPadForTensorCores::Run(HloModule* module) {
  bool changed = false;
  for (HloComputation* comp : module->MakeNonfusionComputations()) {
    for (HloDotInstruction* dot : GetRelevantDots(comp)) {
      TF_ASSIGN_OR_RETURN(bool result, PadForTensorCores(dot));
      changed |= result;
    }
  }
  return changed;
}

}  // namespace gpu
}  // namespace xla
