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

#include "tensorflow/core/transforms/region_to_functional/pass.h"

#include <memory>
#include <utility>

#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/core/transforms/pass_detail.h"
#include "tensorflow/core/transforms/region_to_functional/impl.h"

namespace mlir {
namespace tfg {

namespace {
struct RegionToFunctionalPass
    : public RegionToFunctionalBase<RegionToFunctionalPass> {
  explicit RegionToFunctionalPass(bool force_ctl_capture) {
    force_control_capture = force_ctl_capture;
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    SymbolTable table(getOperation());
    PopulateRegionToFunctionalPatterns(patterns, table, force_control_capture);

    // Iterate until all regions have been outlined. This is guaranteed to
    // terminate because the IR can only hold a finite depth of regions.
    GreedyRewriteConfig config;
    config.maxIterations = GreedyRewriteConfig::kNoIterationLimit;
    if (failed(applyPatternsAndFoldGreedily(getOperation(), std::move(patterns),
                                            config))) {
      getOperation()->emitError(getArgument() + " pass failed");
      signalPassFailure();
    }
  }
};
}  // namespace

std::unique_ptr<Pass> CreateRegionToFunctionalPass(bool force_control_capture) {
  return std::make_unique<RegionToFunctionalPass>(force_control_capture);
}

}  // namespace tfg
}  // namespace mlir
