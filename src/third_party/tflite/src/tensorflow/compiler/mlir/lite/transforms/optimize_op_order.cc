/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/quantization/quantization_traits.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"

namespace mlir {
namespace TFL {
namespace {

// Dequantize ops will produce 3x larger tensors, so we want to move it after
// some passthrough ops to reduce the memory consumption.
struct PushDownDequantize : public OpRewritePattern<DequantizeOp> {
  explicit PushDownDequantize(MLIRContext* context)
      : OpRewritePattern<DequantizeOp>(context) {}

  LogicalResult matchAndRewrite(DequantizeOp dequantize_op,
                                PatternRewriter& rewriter) const override {
    if (!dequantize_op->hasOneUse()) return failure();

    auto use = dequantize_op->use_begin();
    Operation* passthrough_op = use->getOwner();
    unsigned operand_index = use->getOperandNumber();
    if (passthrough_op->hasTrait<OpTrait::IsTerminator>()) return failure();

    auto get_num_elements = [](RankedTensorType tensor) {
      return tensor.getNumElements();
    };

    // If the op is the pass-through op with (3x) smaller output, the dequantize
    // op can be pushed down to the single result of this op.
    if (!llvm::dyn_cast<mlir::SameScalesOpInterface>(passthrough_op) ||
        passthrough_op->getNumResults() != 1) {
      return failure();
    }
    // Only push down the dequantize op when the output is smaller, so that it
    // can have smaller memory usage.
    auto input_type =
        dequantize_op.output().getType().dyn_cast<RankedTensorType>();
    auto output_type =
        passthrough_op->getResult(0).getType().dyn_cast<RankedTensorType>();
    if (!input_type || !output_type || !input_type.hasStaticShape() ||
        !output_type.hasStaticShape() ||
        get_num_elements(input_type) <= get_num_elements(output_type)) {
      return failure();
    }

    // Set the output type of the dequantize op and push it down.
    dequantize_op.output().setType(output_type);
    passthrough_op->replaceAllUsesWith(dequantize_op);

    // Set the input type of the passthrough op and pull it up.
    Type new_output_type =
        QuantizedType::getQuantizedElementType(dequantize_op.input().getType())
            .castFromExpressedType(output_type);
    passthrough_op->getResult(0).setType(new_output_type);
    passthrough_op->setOperand(operand_index, dequantize_op.input());

    // Set the input of the dequantize to the result of the passthrough op.
    // And switch the order of the ops.
    dequantize_op->setOperand(0, passthrough_op->getResult(0));
    dequantize_op->moveAfter(passthrough_op);
    return success();
  }
};

// This transformation pass optimizes the op execution order of the ops in the
// model.
struct OptimizeOpOrderPass
    : public PassWrapper<OptimizeOpOrderPass, FunctionPass> {
  void runOnFunction() override;

  StringRef getArgument() const final {
    // This is the argument used to refer to the pass in
    // the textual format (on the commandline for example).
    return "tfl-optimize-op-order";
  }
  StringRef getDescription() const final {
    // This is a brief description of the pass.
    return "Optimize the execution order of the ops.";
  }
};

void OptimizeOpOrderPass::runOnFunction() {
  OwningRewritePatternList patterns(&getContext());
  auto func = getFunction();
  auto* ctx = func.getContext();
  patterns.insert<PushDownDequantize>(ctx);
  if (failed(applyPatternsAndFoldGreedily(func, std::move(patterns)))) {
    signalPassFailure();
  }
}
}  // namespace

// Creates an instance of the TensorFlow Lite optimize op order pass.
std::unique_ptr<OperationPass<FuncOp>> CreateOptimizeOpOrderPass() {
  return std::make_unique<OptimizeOpOrderPass>();
}

static PassRegistration<OptimizeOpOrderPass> pass;

}  // namespace TFL
}  // namespace mlir
