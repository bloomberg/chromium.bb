/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_COMPARISON_EXPANDER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_COMPARISON_EXPANDER_H_

#include <utility>

#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"
#include "tensorflow/compiler/xla/service/op_expander_pass.h"

namespace xla {

// A pass which performs expansion of the comparison operator to support total
// order comparison of floating point numbers.
class ComparisonExpander : public OpExpanderPass {
 public:
  explicit ComparisonExpander() = default;
  ~ComparisonExpander() override = default;
  absl::string_view name() const override { return "comparison-expander"; }

 private:
  // Returns `true` if `instruction` should be expanded by this pass.
  bool InstructionMatchesPattern(HloInstruction* instruction) override;
  // Returns a replacement for `instruction`, or nullptr if no replacement is
  // needed (e.g. only the to_apply subcomputation of the instruction was
  // modified).
  StatusOr<HloInstruction*> ExpandInstruction(
      HloInstruction* instruction) override;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_COMPARISON_EXPANDER_H_
