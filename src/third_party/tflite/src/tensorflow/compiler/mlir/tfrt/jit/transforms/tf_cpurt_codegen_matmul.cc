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

#include "mlir/Dialect/Linalg/Transforms/CodegenStrategy.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_cpurt_passes.h"

namespace tensorflow {
namespace {

#define GEN_PASS_CLASSES
#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_cpurt_passes.h.inc"

// Use Linalg CodegenStrategy to tile linalg.matmul, linalg.matvec and
// linalg.vecmat operations.
struct CodegenStrategyForMatMulPass
    : public CodegenMatmulBase<CodegenStrategyForMatMulPass> {
  void runOnFunction() override {
    // Promote tiles to full buffers allocated on the stack.
    mlir::linalg::LinalgPromotionOptions full_alloca_promotion;
    full_alloca_promotion.setUseFullTileBuffersByDefault(true).setUseAlloca(
        true);

    // TODO(ezhulenev): Set up tiling options depending on the target machine.

    // Tile and vectorize linalg.matmul operations.
    mlir::linalg::LinalgTilingOptions matmul_tiling;
    matmul_tiling.setTileSizes({12, 32, 16});

    mlir::linalg::CodegenStrategy matmul_strategy;
    matmul_strategy
        .tile(mlir::linalg::MatmulOp::getOperationName(), matmul_tiling)
        .promote(mlir::linalg::MatmulOp::getOperationName(),
                 full_alloca_promotion)
        .vectorize(mlir::linalg::MatmulOp::getOperationName());
    // Created a nested OpPassManager, populate the strategy and run.
    mlir::FuncOp f = getFunction();
    mlir::OpPassManager dynamicPM("builtin.func");
    matmul_strategy.configurePassPipeline(dynamicPM, f.getContext());
    if (failed(runPipeline(dynamicPM, f))) return signalPassFailure();

    // Tile and vectorize linalg.vecmat operations. Interchange loop order to
    // linearly read from the matrix memref.
    mlir::linalg::LinalgTilingOptions vecmat_tiling;
    vecmat_tiling.setTileSizes({16, 8}).setInterchange({1, 0});

    mlir::linalg::CodegenStrategy vecmat_strategy;
    vecmat_strategy
        .tile(mlir::linalg::VecmatOp::getOperationName(), vecmat_tiling)
        .promote(mlir::linalg::VecmatOp::getOperationName(),
                 full_alloca_promotion)
        .vectorize(mlir::linalg::VecmatOp::getOperationName());
    // Created a nested OpPassManager, populate the strategy and run.
    mlir::OpPassManager dynamicPM2("builtin.func");
    vecmat_strategy.configurePassPipeline(dynamicPM2, f.getContext());
    if (failed(runPipeline(dynamicPM2, f))) return signalPassFailure();

    // Vector contraction options.
    mlir::vector::VectorTransformsOptions vector_transforms_ops;
    vector_transforms_ops.setVectorTransformsOptions(
        mlir::vector::VectorContractLowering::OuterProduct);

    // Vector transfer options.
    mlir::VectorTransferToSCFOptions vector_transfer_opts;
    vector_transfer_opts.enableFullUnroll();

    mlir::linalg::CodegenStrategy vector_lowering_strategy;
    vector_lowering_strategy.vectorLowering(
        mlir::linalg::LinalgVectorLoweringOptions()
            .enableTransferPartialRewrite()
            .enableContractionLowering()
            .enableTransferToSCFConversion()
            .setVectorTransformsOptions(vector_transforms_ops)
            .setVectorTransferToSCFOptions(vector_transfer_opts));
    // Created a nested OpPassManager, populate the strategy and run.
    mlir::OpPassManager dynamicPM3("builtin.func");
    vector_lowering_strategy.configurePassPipeline(dynamicPM3, f.getContext());
    if (failed(runPipeline(dynamicPM3, f))) return signalPassFailure();
  }

  void getDependentDialects(mlir::DialectRegistry& registry) const override {
    registry.insert<mlir::vector::VectorDialect>();
  }
};
}  // namespace

std::unique_ptr<mlir::FunctionPass> CreateCodegenStrategyForMatMulPass() {
  return std::make_unique<CodegenStrategyForMatMulPass>();
}

}  // namespace tensorflow
