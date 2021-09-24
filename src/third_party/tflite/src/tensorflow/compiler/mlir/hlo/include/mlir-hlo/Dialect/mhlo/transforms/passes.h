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

#ifndef TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_PASSES_H_
#define TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_PASSES_H_

#include <memory>

#include "llvm/ADT/ArrayRef.h"

namespace mlir {

class FuncOp;
class FunctionPass;
class ModuleOp;
class Operation;
template <typename T>
class OperationPass;
class Pass;
namespace lmhlo {
class FusionOp;
}

namespace mhlo {

/// Lowers HLO control flow ops to the Standard dialect.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeControlFlowPass();

/// Lowers MHLO control flow ops to the SCF dialect.
std::unique_ptr<OperationPass<FuncOp>> createControlFlowToScfPass();

/// Lowers from HLO dialect to Standard dialect.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeToStdPass();

/// Lowers from the CHLO dialect to the HLO dialect.
std::unique_ptr<FunctionPass> createChloLegalizeToHloPass(
    bool legalize_broadcasts = true, bool expand_compositions = true);

// canonicalize reduction ops to be suitable for codegen.
std::unique_ptr<FunctionPass> createHloCanonicalizeReductionPass();

/// Lowers from HLO dialect to LHLO dialect allocating/deallocating temporary
/// buffers if necessary.
std::unique_ptr<OperationPass<ModuleOp>> createLegalizeToLhloPass();

/// Lowers from HLO dialect to Memref dialect allocating/deallocating temporary
/// buffers if necessary.
std::unique_ptr<FunctionPass> createLegalizeToMemrefPass();

// Lowers from HLO dialect to Linalg dialect.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeHloToLinalgPass();

// Place shape calculating subgraph on cpu.
std::unique_ptr<OperationPass<ModuleOp>> createMarkShapeCalcOpPass();

// Sinks constants implicitly captured in control flow regions. This is
// necessary to export to XLA.
std::unique_ptr<OperationPass<FuncOp>> createSinkConstantsToControlFlowPass();

// fuse mhlo ops to kLoop/kInput fusion patterns
std::unique_ptr<OperationPass<FuncOp>> createMhloFusionPass();

/// Lowers trigonometric operations from the standard dialect to approximations
/// that do not use intrinsics.
std::unique_ptr<OperationPass<FuncOp>>
createLegalizeTrigonometricToApproximationPass();

// Move dynamic broadcasts up over element-wise operations and broadcast the
// operands rather than the result. This will eventually allow for larger
// fusions.
std::unique_ptr<FunctionPass> createBroadcastPropagationPass();

/// Rank specialization passes:
///   - Find compatible operations and group them together in one rank
///     specialization cluster.
///   - Lower rank specialization clusters to SCF and ranked operations.
std::unique_ptr<FunctionPass> createRankSpecializationClusterPass();
std::unique_ptr<FunctionPass> createRankSpecializationToSCFPass(
    int64_t max_target_rank = 5);

std::unique_ptr<FunctionPass> createOptimizeMhloPass();
std::unique_ptr<FunctionPass> createLowerComplexPass();
std::unique_ptr<::mlir::Pass> createLegalizeGeneralDotPass();
std::unique_ptr<FunctionPass> createLegalizeEinsumToDotGeneralPass();
std::unique_ptr<FunctionPass> createLegalizeGatherToTorchIndexSelectPass();
std::unique_ptr<FunctionPass> createFlattenTuplePass();

// Creates a pass for expanding mhlo.tuple ops.
std::unique_ptr<OperationPass<ModuleOp>> CreateExpandHloTuplesPass(
    const std::string& entry_function_name = "main");

}  // namespace mhlo

namespace lmhlo {

// Lowers from LHLO dialect to Affine dialect.
std::unique_ptr<OperationPass<FuncOp>> createLhloLegalizeToAffinePass();

// Lowers from LHLO dialect to Linalg dialect.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeLhloToLinalgPass();

// Lowers from LHLO dialect to GPU dialect.
std::unique_ptr<FunctionPass> createLegalizeToGpuPass();

// Fuses linalg ops obtained after LHLO lowering. To enable fusion,
// operations are first tiled.
//
// When 'use_parallel_loops' is set, the tiling will use scf.parallel
// operations. Otherwise, scf.for operations are used.
//
// 'tile_sizes' provides the tile sizes to use for tiling. If the linalg
// operation has more dimensions than tile sizes provided, 1 is used as
// default.
std::unique_ptr<FunctionPass> createLhloFuseLinalgPass(
    bool use_parallel_loops = false, llvm::ArrayRef<unsigned> tile_sizes = {});

// Lowers from LHLO dialect to parallel loops.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeLhloToParallelLoopsPass();

// Legalizes tensor load ops that are inserted during mhlo to lmhlo conversion.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeTensorLoadOpPass();

// fuse lmhlo ops to kLoop/kInput fusion patterns
std::unique_ptr<OperationPass<FuncOp>> createLhloFusionPass(
    int max_num_arguments_per_kernel = 64);

// inline lmhlo.Fusion
std::unique_ptr<OperationPass<FuncOp>> createLhloFusionInlinerPass();

// Lowers the roots of lmhlo.fusion to parallel loops
std::unique_ptr<OperationPass<FuncOp>>
createLhloLegalizeRootsToParallelLoopsPass();

// Input inline fusion pass for fusion codegen
std::unique_ptr<OperationPass<lmhlo::FusionOp>> createInputInlineFusionPass();

}  // namespace lmhlo

namespace disc_ral {

std::unique_ptr<OperationPass<ModuleOp>> createRalInjectExecutionContextPass(
    const std::string& entry_func_name = "main");

// Lower some specific ops to library calls (modeled by `disc_ral.launch` op).
std::unique_ptr<mlir::FunctionPass> createRalLowerToLibraryCallPass();

// Lower disc to llvm dialect
std::unique_ptr<OperationPass<ModuleOp>> createRalToLLVMPass();

}  // namespace disc_ral

}  // namespace mlir

#endif  // TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_PASSES_H_
