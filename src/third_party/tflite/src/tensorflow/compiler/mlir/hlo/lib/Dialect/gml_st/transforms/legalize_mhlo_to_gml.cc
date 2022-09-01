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

#include <utility>

#include "llvm/ADT/SmallVector.h"
#include "mlir-hlo/Dialect/gml_st/IR/gml_st_ops.h"
#include "mlir-hlo/Dialect/gml_st/transforms/pass_detail.h"
#include "mlir-hlo/Dialect/gml_st/transforms/passes.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace mlir {
namespace gml_st {
namespace {

struct DynamicBroadcastInDimOpPattern
    : public OpRewritePattern<mhlo::DynamicBroadcastInDimOp> {
  using OpRewritePattern<mhlo::DynamicBroadcastInDimOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(mhlo::DynamicBroadcastInDimOp op,
                                PatternRewriter& rewriter) const override {
    auto loc = op.getLoc();
    Value output_dimensions = op.output_dimensions();
    auto result_ty = op.getType().cast<RankedTensorType>();

    // Create init tensor as none of the operands are reusable/updatable.
    SmallVector<Value> dynamic_dims;
    SmallVector<int64_t> static_shape_info;
    for (int i = 0; i < result_ty.getRank(); i++) {
      auto i_cst = rewriter.create<arith::ConstantIndexOp>(loc, i);
      dynamic_dims.push_back(rewriter.create<tensor::ExtractOp>(
          loc, output_dimensions, ValueRange{i_cst}));
      static_shape_info.push_back(ShapedType::kDynamicSize);
    }
    auto init_tensor = rewriter.create<linalg::InitTensorOp>(
        loc, dynamic_dims, static_shape_info, result_ty.getElementType());

    rewriter.replaceOpWithNewOp<gml_st::DynamicBroadcastInDimOp>(
        op, result_ty, init_tensor, op.operand(), op.broadcast_dimensions(),
        op.known_expanding_dimensionsAttr(),
        op.known_nonexpanding_dimensionsAttr());
    return success();
  }
};

class LegalizeMHLOToGMLPass
    : public LegalizeMHLOToGMLPassBase<LegalizeMHLOToGMLPass> {
  void getDependentDialects(DialectRegistry& registry) const final {
    registry.insert<GmlStDialect, linalg::LinalgDialect>();
  }

  void runOnOperation() final {
    MLIRContext* ctx = &getContext();
    RewritePatternSet patterns(ctx);

    // List of patterns.
    patterns.insert<DynamicBroadcastInDimOpPattern>(ctx);

    if (failed(applyPatternsAndFoldGreedily(getOperation(),
                                            std::move(patterns)))) {
      return signalPassFailure();
    }
  }
};

}  // namespace

std::unique_ptr<OperationPass<func::FuncOp>> createLegalizeMHLOToGMLPass() {
  return std::make_unique<LegalizeMHLOToGMLPass>();
}

}  // namespace gml_st
}  // namespace mlir
