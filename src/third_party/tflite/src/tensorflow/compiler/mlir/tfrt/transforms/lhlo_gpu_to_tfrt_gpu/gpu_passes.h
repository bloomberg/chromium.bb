// Copyright 2020 The TensorFlow Runtime Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_LHLO_GPU_TO_TFRT_GPU_GPU_PASSES_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_LHLO_GPU_TO_TFRT_GPU_GPU_PASSES_H_

#include <memory>

#include "mlir/Pass/Pass.h"
#include "absl/strings/string_view.h"

namespace tensorflow {

// Creates a pass that lowers lmhlo_gpu ops to tfrt_gpu. Prepares the function
// to be consumed by MLIR's gpu-async-region pass.
std::unique_ptr<mlir::FunctionPass> createLmhloGpuAsyncConversionPass();

// Creates a pass that rewrites a function that has been processed by MLIR's
// gpu-async-region pass so that it can be lowered to bef.
std::unique_ptr<mlir::FunctionPass> createAsyncGpuTfrtConversionPass();

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_LHLO_GPU_TO_TFRT_GPU_GPU_PASSES_H_
