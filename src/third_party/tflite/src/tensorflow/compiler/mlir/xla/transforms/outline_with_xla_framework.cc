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

#include <memory>
#include <stdexcept>
#include <utility>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Constants.h"
#include "mlir/Conversion/ArithmeticToLLVM/ArithmeticToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"  // from @llvm-project
#include "mlir/Conversion/LLVMCommon/Pattern.h"  // from @llvm-project
#include "mlir/Conversion/MathToLLVM/MathToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"  // from @llvm-project
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"  // from @llvm-project
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"  // from @llvm-project
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"  // from @llvm-project
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"  // from @llvm-project
#include "mlir/IR/BlockAndValueMapping.h"  // from @llvm-project
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinDialect.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/OperationSupport.h"  // from @llvm-project
#include "mlir/IR/TypeRange.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/xla/ir/xla_framework.h"
#include "tensorflow/compiler/mlir/xla/transforms/passes.h"
#include "tensorflow/compiler/mlir/xla/transforms/xla_passes_detail.h"

namespace mlir {
namespace mhlo {
namespace {

// Given a FuncOp with only memref args/outputs, create a new function that
// wraps/unwraps xla_framework.buffer types and then calls the original
// function.
//
// For example:
//   func @func_to_outline(%arg0: memref<?xf32>) -> memref<?xf32>
//
// Will generate:
//   func @func_to_outline_xla_framework(%arg0: !xla_framework.buffer)
//     -> !xla_framework.buffer attributes {xla_entry = true} {
//    %0 = xla_framework.buffer_to_mem %arg0 : memref<?xf32>
//    %1 = call @func_to_outline(%0) : (memref<?xf32>) -> memref<?xf32>
//    %2 = xla_framework.mem_to_buffer %1 : memref<?xf32>
//    return %2 : !xla_framework.buffer
//   }
struct OutlineXLAFunc : public RewritePattern {
  explicit OutlineXLAFunc(MLIRContext *context, PatternBenefit benefit = 1)
      : RewritePattern(FuncOp::getOperationName(), benefit, context) {}

  static void filterFuncAttributes(ArrayRef<NamedAttribute> attrs,
                                   bool argAttrs,
                                   SmallVectorImpl<NamedAttribute> &result) {
    for (const auto &attr : attrs) {
      if (attr.getName() == SymbolTable::getSymbolAttrName() ||
          attr.getName() == FunctionOpInterface::getTypeAttrName() ||
          attr.getName() == "std.varargs" ||
          (argAttrs && attr.getName() == FuncOp::getArgDictAttrName()))
        continue;
      result.push_back(attr);
    }
  }

  LogicalResult matchAndRewrite(Operation *op,
                                PatternRewriter &rewriter) const override {
    auto func = dyn_cast<FuncOp>(op);
    auto ctx = rewriter.getContext();
    auto loc = func.getLoc();
    SmallVector<Location> locs(func.getType().getNumInputs(), loc);

    // Functions should only be outlined once and should only use memrefs
    if (!func) return failure();
    if (llvm::any_of(op->getOperandTypes(),
                     [](Type t) { return !t.isa<MemRefType>(); }) ||
        op->getNumResults() != 0)
      return failure();
    if (func->hasAttr("outlined")) return failure();
    func->setAttr("outlined", BoolAttr::get(ctx, true));

    // Prepare new func attribute information
    func.setSymNameAttr(mlir::StringAttr::get(ctx, func.getName()));
    SmallVector<Type> operands(func.getType().getNumInputs(),
                               ::mlir::xla_framework::BufferType::get(ctx));
    SmallVector<Type> result_array(func.getType().getNumResults(),
                                   ::mlir::xla_framework::BufferType::get(ctx));
    auto func_type = FunctionType::get(ctx, operands, result_array);
    SmallVector<NamedAttribute> attrs;
    filterFuncAttributes(func->getAttrs(), true, attrs);
    SmallVector<DictionaryAttr> arg_attrs;
    func.getAllArgAttrs(arg_attrs);

    // The wrapper function will have the same name but with _xla_framework
    // appended and will be annotated with the attribute "xla_entry".
    auto outline_func =
        rewriter.create<FuncOp>(loc, func.getSymName().str() + "_xla_framework",
                                func_type, attrs, arg_attrs);
    outline_func->setAttr("outlined", BoolAttr::get(ctx, true));
    outline_func->setAttr("xla_entry", BoolAttr::get(ctx, true));
    auto *b = rewriter.createBlock(&outline_func.getBody(), {},
                                   func_type.getInputs(), locs);

    // Unwrap arguments
    SmallVector<Value> args;
    for (const auto &t : llvm::enumerate(func.getType().getInputs())) {
      args.push_back(rewriter.create<xla_framework::XLABufferToMemOp>(
          loc, t.value(), b->getArgument(t.index())));
    }

    auto call = rewriter.create<func::CallOp>(
        loc, func.getSymName(), func.getType().getResults(), args);
    // Wrap results
    SmallVector<Value> results;
    for (auto t : call.getResults()) {
      results.push_back(rewriter.create<xla_framework::MemToXLABufferOp>(
          loc, ::mlir::xla_framework::BufferType::get(ctx), t));
    }

    rewriter.create<func::ReturnOp>(loc, results);
    return success();
  }
};

class OutlineWithXLAFrameworkPass
    : public OutlineWithXLAFrameworkBase<OutlineWithXLAFrameworkPass> {
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<xla_framework::XLAFrameworkDialect, mlir::BuiltinDialect>();
  }

 public:
  explicit OutlineWithXLAFrameworkPass() {}

  void runOnOperation() override {
    ModuleOp m = getOperation();

    // Populate type conversions.
    MLIRContext *ctx = m.getContext();

    // Populate patterns.
    RewritePatternSet patterns(&getContext());
    patterns.add<OutlineXLAFunc>(ctx);
    //  Set target.

    if (failed(applyPatternsAndFoldGreedily(m, std::move(patterns)))) {
      signalPassFailure();
    }
    m->walk([](FuncOp f) {
      if (f->hasAttr("outlined")) f->removeAttr("outlined");
    });
  }
};

}  // namespace

std::unique_ptr<OperationPass<ModuleOp> > CreateOutlineWithXLAFrameworkPass() {
  return std::make_unique<OutlineWithXLAFrameworkPass>();
}

}  // namespace mhlo
}  // namespace mlir
