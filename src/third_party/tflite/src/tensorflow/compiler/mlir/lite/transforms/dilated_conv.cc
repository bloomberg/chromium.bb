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
#include "tensorflow/compiler/mlir/lite/transforms/dilated_conv.h"

#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project

namespace mlir {
namespace TFL {
namespace {

struct IdentifyDilatedConvPass
    : public PassWrapper<IdentifyDilatedConvPass, FunctionPass> {
  void runOnFunction() override;

  StringRef getArgument() const final {
    // This is the argument used to refer to the pass in
    // the textual format (on the commandline for example).
    return "tfl-identify-dilated-conv";
  }
  StringRef getDescription() const final {
    // This is a brief description of the pass.
    return "Identify and replace patterns for dilated convolution.";
  }
};

void IdentifyDilatedConvPass::runOnFunction() {
  OwningRewritePatternList patterns(&getContext());
  auto func = getFunction();

  patterns.insert<ConvertTFDilatedConvOp<TF::Conv2DOp>,
                  ConvertTFDilatedConvOp<TF::DepthwiseConv2dNativeOp>>(
      &getContext());
  (void)applyPatternsAndFoldGreedily(func, std::move(patterns));
}
}  // namespace

static PassRegistration<IdentifyDilatedConvPass> pass;

}  // namespace TFL
}  // namespace mlir
