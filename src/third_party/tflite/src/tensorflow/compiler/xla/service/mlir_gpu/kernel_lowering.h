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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_KERNEL_LOWERING_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_KERNEL_LOWERING_H_

#include "mlir/IR/Module.h"  // from @llvm-project
#include "tensorflow/compiler/xla/status.h"
#include "tensorflow/compiler/xla/statusor.h"

namespace xla {
namespace mlir_gpu {

struct LowerLHLOToGPUOptions {
  llvm::ArrayRef<unsigned> tile_sizes = {16, 64};
  llvm::ArrayRef<unsigned> unroll_factors = {};
  bool collapse_parallel_loops = true;
  bool rewrite_signature = true;
  bool use_approximations = false;
};

Status LowerLHLOToGPU(mlir::ModuleOp module,
                      LowerLHLOToGPUOptions options = {});

Status LowerKernelBodiesToNVVM(mlir::ModuleOp module);

StatusOr<mlir::ModuleOp> ExtractKernelModule(mlir::ModuleOp module);

}  // namespace mlir_gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_KERNEL_LOWERING_H_
