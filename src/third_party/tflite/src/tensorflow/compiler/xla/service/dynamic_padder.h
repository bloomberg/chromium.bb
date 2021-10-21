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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_DYNAMIC_PADDER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_DYNAMIC_PADDER_H_

#include "tensorflow/compiler/xla/service/dynamic_dimension_inference.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"

namespace xla {

// With bounded shapes, only part of the shape contains effective data and the
// rest contains padded data, whose value can be anything depending on the
// source of the data. When a bounded shape is directly consumed by an
// instruction that collapses dimensions (reduce for example), the padding data
// would affect result of the instruction.
//
// DynamicPadder uses DynamicDimensionInference to detect bounded shapes in a
// hlo module, it then inserts certain instructions to reset the padding into an
// identity value so that in doesn't affect the result of subsequent
// instruction. For example, it'd reset the padding to 0 before a bounded shape
// is consumed by a reduce-sum.
//
// Dynamic_padder removes dynamic shapes from the entry computation, and inserts
// custom calls (with dynamic shapes), which are lowered by specialized
// emitters: PadToStatic and SliceToDynamic.

// Each instruction can have one of the three modes in supporting dynamic
// lowering.
enum OpDynamismSupport {
  // There is no support for dynamic lowering -- dynamic padder will make sure
  // the input to that op has static bound by rewriting the op (e.g, extra space
  // in reduce_sum will be padded with 0).
  kNoSupport = 0,
  // The op can take either dynamic input or static input.
  kOptional,
  // The op only has a dynamic lowering, dynamic padder will make sure the input
  // to this op is in dynamic form.
  kRequired,
};

struct DynamicPadderOptions {
  // Returns true if given instruction supports native dynamic lowering. If
  // so, dynamic padder will not attempt to pad it.
  using OpSupportsDynamismHandler =
      std::function<OpDynamismSupport(HloInstruction*)>;

  OpSupportsDynamismHandler op_supports_dynamism_handler = nullptr;

  // Instruct how to inference output dynamic dimensions of custom calls.
  DynamicDimensionInference::CustomCallInferenceHandler custom_call_handler =
      nullptr;

  // If `slice_dynamic_output` is true, insert 'slice_to_dynamic' ops to all
  // outputs that are inferred to be dynamic.
  bool slice_dynamic_output = true;

  // If set to true, pessimisticly assumes runtime shape checks may fail and
  // returns a compile-time error.
  DynamicDimensionInference::ShapeCheckMode shape_check_mode =
      DynamicDimensionInference::ShapeCheckMode::kIgnore;
};

class DynamicPadder : public HloModulePass {
 public:
  explicit DynamicPadder(DynamicPadderOptions options = DynamicPadderOptions())
      : options_(options) {}

  absl::string_view name() const override { return "dynamic_padder"; }

  StatusOr<bool> Run(HloModule* module) override;

 private:
  DynamicPadderOptions options_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_DYNAMIC_PADDER_H_
