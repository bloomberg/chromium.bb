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

#ifndef TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_LHLO_ELEMENTAL_UTILS_H_
#define TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_LHLO_ELEMENTAL_UTILS_H_

#include "mlir/IR/Builders.h"

namespace mlir {

class Value;
class Location;
class Operation;
class ValueRange;
class Region;
class FuncOp;
enum class AtomicRMWKind : uint64_t;

namespace scf {
class ForOp;
class ParallelOp;
}  // namespace scf

namespace memref {
class LoadOp;
}

namespace lmhlo {

Value createLoadOrUseCachedValue(Location loc, OpBuilder* b, Value memref,
                                 ValueRange indices,
                                 OpBuilder::InsertPoint insert_point);

DenseSet<Operation*> NoLoaderUser(SmallVectorImpl<Operation*>& ops);
void cleanUnusedLhloOps(Block* parent);

template <typename LHLO_OpTy>
Value elementalLower(OpBuilder* b, Location loc, LHLO_OpTy op,
                     ValueRange output_index, bool check_cache = false);

scf::ForOp createLoopAndSetInsPt(OpBuilder& b, Location loc, Value& var,
                                 Value lb, Value ub, Value step,
                                 ArrayRef<Value> init_values = {});

scf::ParallelOp createParallelAndSetInsPt(OpBuilder& b, Location loc,
                                          SmallVectorImpl<Value>& vars,
                                          ArrayRef<Value> lbs,
                                          ArrayRef<Value> ubs,
                                          ArrayRef<Value> steps,
                                          ArrayRef<Value> init_values);

void createOffsetStore(OpBuilder& b, Location loc, Value res, Value memref,
                       Value offset);

memref::LoadOp createOffsetLoad(OpBuilder& b, Location loc, Value memref,
                                Value offset);

}  // namespace lmhlo
}  // namespace mlir

#endif  // TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_MHLO_TRANSFORMS_LHLO_ELEMENTAL_UTILS_H_
