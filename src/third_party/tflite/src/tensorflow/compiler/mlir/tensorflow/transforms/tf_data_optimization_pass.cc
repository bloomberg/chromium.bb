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

#include <memory>

#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes_detail.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_data_optimization.h"

namespace mlir {
namespace TF {
namespace {

// Perform tf.data optimizations.
struct TFDataOptimization
    : public TFDataOptimizationPassBase<TFDataOptimization> {
  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    mlir::TF::PopulateTFDataOptimizationPatterns(&getContext(), &patterns);

    (void)applyPatternsAndFoldGreedily(getOperation(), std::move(patterns));
  }
};

}  // namespace

std::unique_ptr<OperationPass<FuncOp>> CreateTFDataOptimizationPass() {
  return std::make_unique<TFDataOptimization>();
}

}  // namespace TF
}  // namespace mlir
