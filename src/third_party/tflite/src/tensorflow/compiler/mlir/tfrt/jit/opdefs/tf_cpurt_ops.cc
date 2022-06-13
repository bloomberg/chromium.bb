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

#include "tensorflow/compiler/mlir/tfrt/jit/opdefs/tf_cpurt_ops.h"

#include <algorithm>

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/TypeUtilities.h"
#include "tfrt/basic_kernels/opdefs/types.h"  // from @tf_runtime

namespace mlir {
namespace tf_cpurt {

//===----------------------------------------------------------------------===//
// CpuRuntimeDialect Dialect
//===----------------------------------------------------------------------===//

CpuRuntimeDialect::CpuRuntimeDialect(mlir::MLIRContext* context)
    : Dialect(/*name*/ "tf_cpurt", context,
              mlir::TypeID::get<CpuRuntimeDialect>()) {
  addOperations<
#define GET_OP_LIST
#include "tensorflow/compiler/mlir/tfrt/tf_cpurt_ops.cc.inc"
      >();
}

// Computes the number of elements in the tensor type. Optimistically use `1` as
// a size of all unknown dimensions. These heuristics match cost estimates of
// the fallback_async::ExecuteOp operations.
static int64_t GetRankedTensorSize(TensorType tensor) {
  assert(tensor.hasRank() && "shape must be ranked");
  if (!tensor.hasRank()) return 0;

  int64_t size = 1;  // scalars (rank 0) have size 1
  for (int64_t dim : tensor.getShape()) size *= std::max<int64_t>(1, dim);
  return size;
}

int64_t FallbackExecuteOp::cost() {
  Operation* self = getOperation();

  // Find the referenced kernel function.
  auto kernel_fn = SymbolTable::lookupNearestSymbolFrom<FuncOp>(self, kernel());
  if (!kernel_fn) return 1;

  int64_t cost = 0;

  // Get the sum of sizes of all ranked inputs.
  //
  // TODO(ezhulenev): Once we have a proper cost model for MLIR operations,
  // use it to compute a more precise cost estimation.
  for (Type type : kernel_fn.getArgumentTypes()) {
    TensorType tensor = type.dyn_cast<TensorType>();
    if (!tensor || !tensor.hasRank()) continue;

    cost += GetRankedTensorSize(tensor);
  }

  // Scale the cost by the number of operations in the function body. The choice
  // of log2 function is arbitrary, seems to work well in benchmarks.
  double scale = std::log2(kernel_fn.body().front().getOperations().size());
  cost *= scale;

  return std::max<int64_t>(1, cost);
}

}  // namespace tf_cpurt
}  // end namespace mlir

#define GET_OP_CLASSES
#include "tensorflow/compiler/mlir/tfrt/tf_cpurt_ops.cc.inc"
