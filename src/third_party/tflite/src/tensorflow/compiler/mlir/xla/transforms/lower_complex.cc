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

// Thsi file implements passes to convert complex operations to equivalent real
// value operations. This does not include removing complex values from function
// argument or return types.

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>

#include "llvm/ADT/STLExtras.h"
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/TypeUtilities.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/xla/ir/hlo_ops.h"
#include "tensorflow/compiler/mlir/xla/ir/hlo_utils.h"
#include "tensorflow/compiler/mlir/xla/transforms/passes.h"

using mlir::FunctionPass;
using mlir::OwningRewritePatternList;
using mlir::PassRegistration;
using mlir::PassWrapper;

namespace {
class LowerComplex : public PassWrapper<LowerComplex, FunctionPass> {
 public:
  explicit LowerComplex() : PassWrapper<LowerComplex, FunctionPass>() {}

  /// Performs the lowering to XLA dialect.
  void runOnFunction() override;
};
}  // end anonymous namespace

namespace mlir {
namespace xla {
namespace {

#include "tensorflow/compiler/mlir/xla/transforms/generated_lower_complex.inc"

}  // end anonymous namespace

void PopulateComplexLoweringPatterns(MLIRContext* context,
                                     OwningRewritePatternList* patterns) {
  populateWithGenerated(context, patterns);
}
}  // end namespace xla
}  // end namespace mlir

// Lowers the complex operations that can be represented using other operations.
void LowerComplex::runOnFunction() {
  // Add lowering patterns to the list.
  OwningRewritePatternList patterns;
  mlir::xla::PopulateComplexLoweringPatterns(&getContext(), &patterns);

  applyPatternsAndFoldGreedily(getFunction(), patterns);
}

static PassRegistration<LowerComplex> pass(
    "test-xla-lower-complex",
    "Lower complex operations into non-complex operations");
