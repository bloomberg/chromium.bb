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

#ifndef TENSORFLOW_COMPILER_MLIR_XLA_IR_HLO_UTILS_H_
#define TENSORFLOW_COMPILER_MLIR_XLA_IR_HLO_UTILS_H_

#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/IR/TypeUtilities.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/xla/convert_op_folder.h"

namespace mlir {
namespace xla {

// Computes the broadcast dimensions attr for an elementwise binary operator
// between two ranked tensors.
// If `allow_empty` is true, then null can be returned to mean that the
// broadcast is an "identity".
mlir::DenseIntElementsAttr getBroadcastDimensionsAttr(mlir::Builder* b,
                                                      mlir::Value x,
                                                      mlir::Value y,
                                                      bool allow_empty = true);

// Get a constant splat for the given value of type. Requires value to be of
// type static shaped RankedTensorType.
template <typename T>
static ElementsAttr getSplat(Builder* b, RankedTensorType ty, T constant) {
  Type element_ty = getElementTypeOrSelf(ty);

  if (element_ty.isSignlessInteger())
    return DenseElementsAttr::get(ty, b->getIntegerAttr(element_ty, constant));

  if (element_ty.isa<FloatType>())
    return DenseElementsAttr::get(ty, b->getFloatAttr(element_ty, constant));

  if (auto complex_ty = element_ty.dyn_cast<ComplexType>()) {
    auto complex_element_ty = complex_ty.getElementType();
    if (complex_element_ty.isF32())
      return DenseElementsAttr::get(ty,
                                    static_cast<std::complex<float>>(constant));
    if (complex_element_ty.isF64())
      return DenseElementsAttr::get(
          ty, static_cast<std::complex<double>>(constant));
  }
  llvm_unreachable("unhandled element type");
}

template <typename T>
static ElementsAttr getSplat(Builder* b, Value val, T constant) {
  return getSplat(b, val.getType().cast<RankedTensorType>(), constant);
}

// Returns DenseElementsAttr of rank zero with the given element type and the
// value.
// Requires `ty` to be either FloatType of IntegerType.
DenseElementsAttr GetScalarOfType(Type ty, int64_t raw_value);

}  // namespace xla
}  // namespace mlir

#endif  // TENSORFLOW_COMPILER_MLIR_XLA_IR_HLO_UTILS_H_
