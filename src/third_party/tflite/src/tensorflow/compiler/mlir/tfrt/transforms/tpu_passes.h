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

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_TPU_PASSES_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_TPU_PASSES_H_

// This file contains stub implementations for Google internal TPU APIs.

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Pass/PassOptions.h"

namespace tensorflow {

class CoreRTConverter;

struct TfrtTpuCompileOptions
    : mlir::PassPipelineOptions<TfrtTpuCompileOptions> {
  Option<bool> move_resource_gather_to_host{
      *this, "move-resource-gather-to-host",
      llvm::cl::desc("Move resource gather ops to host"),
      llvm::cl::init(false)};
};

struct TfrtTpuExecuteOpConversionOptions {
  bool use_core_selector;
  bool use_bundled_transfer;
  bool transfer_result_to_host;
};

// Registers a set of dialects used in TFRT TPU lowering.
inline void RegisterTPUDialects(mlir::DialectRegistry *registry) {}

// Adds a target dialect and a set of rewrite patterns for TFRT TPU lowering.
inline void AddTPUTargetDialectAndPatterns(
    mlir::ConversionTarget *target, mlir::OwningRewritePatternList *patterns,
    mlir::MLIRContext *context, CoreRTConverter *corert_converter,
    const TfrtTpuExecuteOpConversionOptions &tpu_exec_conv_opts,
    bool tpu_lower_to_fallback) {}

// Rewrites specific TF TPU ops to equivalent TF ops in a module.
inline mlir::LogicalResult RunTPUBackwardCompatConversion(
    mlir::ModuleOp module, const TfrtTpuCompileOptions &options) {
  return mlir::failure();
}

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_TPU_PASSES_H_
