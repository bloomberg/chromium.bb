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

// This file implements logic for translating mixed IR to buffer form.
// Currently it supports MHLO and some operations from the Standard dialect.

#include <memory>
#include <utility>

#include "llvm/ADT/STLExtras.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/transforms/type_conversion.h"
#include "tensorflow/compiler/mlir/tools/kernel_gen/transforms/passes.h"

namespace mlir {
namespace kernel_gen {
namespace transforms {
namespace {

// Generic pattern that rewrites any op by rewriting its operands and result
// types. Regions are also rewritten.
class ConvertToSignless : public ConversionPattern {
 public:
  ConvertToSignless(TypeConverter& type_converter, MLIRContext* context)
      : ConversionPattern(type_converter, MatchAnyOpTypeTag{}, 0, context) {}

  LogicalResult matchAndRewrite(
      Operation* op, ArrayRef<Value> operands,
      ConversionPatternRewriter& rewriter) const final {
    SmallVector<Type> result_types;
    if (failed(typeConverter->convertTypes(op->getResultTypes(), result_types)))
      return failure();

    auto* new_op = Operation::create(op->getLoc(), op->getName(), result_types,
                                     operands, op->getAttrs(),
                                     op->getSuccessors(), op->getNumRegions());
    for (auto regions : llvm::zip(op->getRegions(), new_op->getRegions())) {
      Region& before = std::get<0>(regions);
      Region& parent = std::get<1>(regions);
      rewriter.inlineRegionBefore(before, parent, parent.end());
      if (failed(rewriter.convertRegionTypes(&parent, *typeConverter)))
        return failure();
    }
    rewriter.insert(new_op);
    rewriter.replaceOp(op, new_op->getResults());
    return success();
  }
};

#define GEN_PASS_CLASSES
#include "tensorflow/compiler/mlir/tools/kernel_gen/transforms/kernel_gen_passes.h.inc"

struct ConvertToSignlessPass
    : public ConvertToSignlessPassBase<ConvertToSignlessPass> {
 public:
  void runOnOperation() override {
    auto& context = getContext();
    ConversionTarget target(context);

    mhlo::RemoveSignTypeConverter converter;
    target.markUnknownOpDynamicallyLegal([&](auto op) {
      return converter.isLegal(op->getOperandTypes()) &&
             converter.isLegal(op->getResultTypes());
    });
    target.addDynamicallyLegalOp<func::FuncOp>([&](func::FuncOp op) {
      return converter.isSignatureLegal(op.getFunctionType());
    });

    RewritePatternSet patterns(&getContext());
    patterns.add<ConvertToSignless>(converter, &context);
    // FuncOp is special as it has type encoding via attributes.
    populateFunctionOpInterfaceTypeConversionPattern<func::FuncOp>(patterns,
                                                                   converter);

    auto module = getOperation();
    if (failed(applyFullConversion(module, target, std::move(patterns)))) {
      signalPassFailure();
    }
  }
};

}  // namespace

std::unique_ptr<OperationPass<ModuleOp>> CreateConvertToSignlessPass() {
  return std::make_unique<ConvertToSignlessPass>();
}

}  // namespace transforms
}  // namespace kernel_gen
}  // namespace mlir
