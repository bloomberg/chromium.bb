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

#ifndef MLIR_HLO_DIALECT_GML_ST_TRANSFORMS_PASSES_H
#define MLIR_HLO_DIALECT_GML_ST_TRANSFORMS_PASSES_H

#include <memory>

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/Pass.h"

namespace mlir {
namespace gml_st {

/// Experimental pass to lower MHLO to destination-style ops in GML and linalg.
std::unique_ptr<OperationPass<func::FuncOp>> createLegalizeMHLOToGMLPass();

/// Experimental pass to fuse producers into `gml_st.materialize` ops.
std::unique_ptr<OperationPass<func::FuncOp>> createFusionPass();

/// Create a pass to convert `gml_st.loop` to `scf.for` and `scf.parallel`
/// loops and memref.load/memref.store accesses.
std::unique_ptr<OperationPass<func::FuncOp>> createGmlStToScfPass();

// Pass to bufferize `linalg.tiled_loop` including the operations contained in
// its body.
std::unique_ptr<OperationPass<func::FuncOp>> CreateTiledLoopBufferizePass();

#define GEN_PASS_REGISTRATION
#include "mlir-hlo/Dialect/gml_st/transforms/passes.h.inc"

}  // namespace gml_st
}  // namespace mlir

#endif  // MLIR_HLO_DIALECT_GML_ST_TRANSFORMS_PASSES_H
