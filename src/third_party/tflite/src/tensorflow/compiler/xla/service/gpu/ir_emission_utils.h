/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_IR_EMISSION_UTILS_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_IR_EMISSION_UTILS_H_

#include <string>
#include <utility>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/lhlo/IR/lhlo_ops.h"
#include "tensorflow/compiler/xla/service/buffer_assignment.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"

namespace xla {
namespace gpu {

// Matrix multiplication before the rewrite.
//
// This function should never return "true" on instructions after
// GemmRewriter pass has finished.
bool IsMatrixMultiplication(const HloInstruction& dot);

inline constexpr int64_t WarpSize() { return 32; }

// Need at least 1024 threads/block for reasonable tree reduction
// performance (assuming all data fits).
inline constexpr int64_t MinThreadsXRowReduction() { return 1024; }

// When doing batched row reduction, how big the batch dimension could be.
inline constexpr int64_t BatchedReductionRaceFreeBound() { return 8; }

// Returns true if `hlo` will be implemented as a call to a cuSolver routine.
//
// This returns true if `hlo` is a CustomCall HLO with a call target equal to
// one of the kCusolver... constants, but returns *false* for HLOs with
// say, a kCholesky opcode.
bool IsCustomCallToCusolver(const HloInstruction& hlo);

// Cholesky decomposition. Takes a (batched) matrix as input, and returns a
// tuple of (result, workspace, info), where result is the result of the
// Cholesky decomposition, workspace is scratch space for cuSolver, and info
// is a success/failure code per batch element.
extern const char* const kCusolverCholeskyCallTarget;

// Returns true if either the dimensions being reduced or the dimensions being
// kept are contiguous in the input of the reduce instruction.
bool IsReductionFromOrToContiguousDimensions(const HloInstruction& reduce);

// MLIR variant.
bool IsReductionFromOrToContiguousDimensions(mlir::Operation* op);

// Returns whether unnested_hlo is an input fusion whose root is either a slice
// or a tuple of slices. If verify_no_strides is true, returns false unless all
// ROOT slices have no strides.
bool IsInputFusibleSlices(mlir::Operation* unnested_hlo,
                          bool verify_no_strides);

struct ReductionDimensions {
  // Indicates whether the reduction is a row reduction or a column reduction.
  bool is_row_reduction;

  // Contains the size of the three contiguous components for
  // the reduction [depth, height, width] (major-to-minor ordering).
  //
  // For row reduction, we do: [D, H, W] -> [D, H].
  // For column reduction, we do: [D, H, W] -> [D, W].
  std::array<int64_t, 3> dimensions;
};

// Given the input shape and dimensions to reduce for a reduction, returns
// ReductionDimensions.
//
// Prerequisite: the reduction instruction passes the check
// IsReductionFromOrToContiguousDimensions, which guarantees either the
// dimensions to reduce or the dimensions to keep are consecutive.
ReductionDimensions GetReductionKindAndContiguousComponents(
    const HloInstruction& reduce);
ReductionDimensions GetReductionKindAndContiguousComponents(
    mlir::Operation* reduce);

// Get tiling per thread for the given reduction in dimensions [D, H, W].
std::array<int64_t, 3> GetReductionTiling(
    const ReductionDimensions& reduction_dimensions,
    se::CudaComputeCapability cuda_compute_capability);

// Emits call to "vprintf" with given format and arguments.
llvm::Value* EmitPrintf(absl::string_view fmt,
                        absl::Span<llvm::Value* const> arguments,
                        llvm::IRBuilder<>* builder);

// Emits code to shuffle data between threads of a warp. This has the same
// semantics as the PTX "shfl.sync.down" instruction but works for values that
// aren't 32 bits in size. The last operand of the emitted "shfl" is
// `WarpSize() - 1`.
//
// This function emits a "full-warp" shuffle, which all threads of a warp
// participate in.  *Do not use this function from a divergent context:* You
// can't correctly do so on both Volta and earlier GPUs.
//
// https://docs.nvidia.com/cuda/parallel-thread-execution/#data-movement-and-conversion-instructions-shfl-sync
llvm::Value* EmitFullWarpShuffleDown(llvm::Value* value, llvm::Value* offset,
                                     llvm::IRBuilder<>* builder);

// Emits code that determines whether the current thread is thread 0 within
// block 0 of the kernel.
llvm::Value* IsBlock0Thread0(llvm::IRBuilder<>* b);

// Returns whether the output of a fusion with reduction are consistent with
// `first_reduce`.
bool IsFusedReductionOutputConsistent(const HloInstruction* inst,
                                      const HloInstruction* first_reduce);
inline bool AreFusedReductionOutputsConsistent(
    absl::Span<const HloInstruction* const> output_instructions,
    const HloInstruction* first_reduce) {
  return absl::c_all_of(output_instructions, [=](const HloInstruction* inst) {
    return IsFusedReductionOutputConsistent(inst, first_reduce);
  });
}

inline std::string MlirToString(mlir::Operation* op) {
  std::string s;
  {
    llvm::raw_string_ostream os(s);
    op->print(os);
  }
  return s;
}

inline std::string MlirToString(const mlir::Location& loc) {
  std::string s;
  {
    llvm::raw_string_ostream os(s);
    loc.print(os);
  }
  return s;
}

int PartitionLmhloOperandsAndOutputs(mlir::Operation* op);
std::vector<mlir::Value> GetHloOperands(mlir::Operation* op);
std::vector<mlir::Value> GetHloOutputs(mlir::Operation* op);

bool WritesMlirBuffer(mlir::Operation* op, mlir::Value operand);

template <typename T>
std::vector<T> ToStdVector(const llvm::SmallVectorImpl<T>& v) {
  return std::vector<T>(v.begin(), v.end());
}

StatusOr<BufferAllocation::Slice> GetAllocationSlice(
    mlir::Value v, absl::Span<const BufferAllocation> allocations,
    std::string* constant_name = nullptr);

bool CanEmitFusedDynamicUpdateSliceInPlaceForGpu(
    mlir::lmhlo::FusionOp fusion,
    absl::Span<const BufferAllocation> allocations);

Shape GetShape(mlir::Value value);

// Returns whether the given reduction can be safely generated without atomics:
// that is, at most one block will write to every output element.
bool ReductionIsRaceFree(const ReductionDimensions& reduction_dimensions,
                         const std::array<int64_t, 3>& reduction_tiling);

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_IR_EMISSION_UTILS_H_
