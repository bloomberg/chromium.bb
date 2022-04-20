/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/transforms/functional_to_region/pass.h"

#include <memory>
#include <utility>

#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/SymbolTable.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/core/transforms/functional_to_region/impl.h"
#include "tensorflow/core/transforms/pass_detail.h"

namespace mlir {
namespace tfg {

namespace {
struct FunctionalToRegionPass
    : public FunctionalToRegionBase<FunctionalToRegionPass> {
  void runOnOperation() override {
    SymbolTable table(getOperation());
    RewritePatternSet patterns(&getContext());
    PopulateFunctionalToRegionPatterns(patterns, table);

    // Allow instantiation of a "large" depth of nested control-flow ops.
    // TODO(jeffniu): This number was picked arbitrarily.
    GreedyRewriteConfig config;
    config.maxIterations = 10000;
    if (failed(applyPatternsAndFoldGreedily(getOperation(), std::move(patterns),
                                            config)))
      signalPassFailure();
  }
};
}  // namespace

std::unique_ptr<Pass> CreateFunctionalToRegionPass() {
  return std::make_unique<FunctionalToRegionPass>();
}

}  // namespace tfg
}  // namespace mlir
