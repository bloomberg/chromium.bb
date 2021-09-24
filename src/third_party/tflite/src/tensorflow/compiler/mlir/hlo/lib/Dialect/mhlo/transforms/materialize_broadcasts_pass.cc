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

#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir-hlo/Dialect/mhlo/transforms/PassDetail.h"
#include "mlir-hlo/Dialect/mhlo/transforms/rewriters.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

namespace mlir {
namespace mhlo {

namespace {

struct TestMaterializeBroadcastsPass
    : public TestMaterializeBroadcastsPassBase<TestMaterializeBroadcastsPass> {
  void runOnFunction() override {
    ConversionTarget conversionTarget(getContext());
    OwningRewritePatternList conversionPatterns(&getContext());

    // Consider the mhlo dialect legal for tests.
    conversionTarget.addLegalDialect<MhloDialect>();
    // The conversion uses helpers from the Standard dialect.
    conversionTarget.addLegalDialect<mlir::StandardOpsDialect>();

    SetupMaterializeBroadcastsLegality(&getContext(), &conversionTarget);
    PopulateMaterializeBroadcastsPatterns(&getContext(), &conversionPatterns);

    if (failed(applyPartialConversion(getFunction(), conversionTarget,
                                      std::move(conversionPatterns)))) {
      return signalPassFailure();
    }
  }
};

}  // namespace

std::unique_ptr<::mlir::Pass> createTestMaterializeBroadcastsPass() {
  return std::make_unique<TestMaterializeBroadcastsPass>();
}

}  // namespace mhlo
}  // namespace mlir
