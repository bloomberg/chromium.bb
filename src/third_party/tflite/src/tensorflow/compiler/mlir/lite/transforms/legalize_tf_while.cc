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

// Converts TF While to TFL While with single call in body and cond.

#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

namespace mlir {
namespace TFL {
namespace {

// Legalize TF While to TFL While with calls to the original functions from the
// cond and body regions.
struct LegalizeWhile
    : public PassWrapper<LegalizeWhile, OperationPass<ModuleOp>> {
  void getDependentDialects(DialectRegistry& registry) const override {
    registry.insert<TFL::TensorFlowLiteDialect>();
  }

  StringRef getArgument() const final {
    // This is the argument used to refer to the pass in
    // the textual format (on the commandline for example).
    return "tfl-legalize-tf-while";
  }
  StringRef getDescription() const final {
    // This is a brief description of the pass.
    return "Legalize from TensorFlow While to TensorFlow Lite While";
  }

  void RunOnFunction(FuncOp func);

  void runOnOperation() override {
    for (auto op : getOperation().getOps<FuncOp>()) RunOnFunction(op);
  }
};

}  // namespace

// Inserts call to the given function into the 'region'.
void CreateRegionWithCall(FuncOp func, Region& region, Location loc) {
  OpBuilder builder(region);
  auto block = builder.createBlock(&region);
  SmallVector<Value, 4> new_operands;
  for (Type t : func.getType().getInputs())
    new_operands.push_back(block->addArgument(t));
  auto call = builder.create<CallOp>(loc, func, new_operands);
  builder.create<YieldOp>(loc, call.getResults());
  // Mark old function as private so that it can be DCE'd if not called.
  func.setPrivate();
}

void RunOnWhile(TF::WhileOp while_op) {
  Operation* op = while_op.getOperation();
  // Create new TFL While op that will be used to replace TF While op.
  auto new_op = OpBuilder(op).create<TFL::WhileOp>(
      op->getLoc(), op->getResultTypes(), op->getOperands(),
      while_op.is_stateless());
  Location loc = while_op->getLoc();
  CreateRegionWithCall(while_op.cond_function(), new_op.cond(), loc);
  CreateRegionWithCall(while_op.body_function(), new_op.body(), loc);

  op->replaceAllUsesWith(new_op.getResults());
  op->erase();
}

void LegalizeWhile::RunOnFunction(FuncOp func) {
  // Convert all TF WhileOps inside the function body to TFL While ops.
  func.getBody().walk([](TF::WhileOp while_op) { RunOnWhile(while_op); });
}

// Creates an instance of the TensorFlow While to TFLite While pass.
std::unique_ptr<OperationPass<ModuleOp>> CreateLegalizeTFWhilePass() {
  return std::make_unique<LegalizeWhile>();
}

static PassRegistration<LegalizeWhile> pass;

}  // namespace TFL
}  // namespace mlir
