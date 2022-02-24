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

// This file defines the operations used in the MHLO dialect.

#ifndef MLIR_HLO_DIALECT_MHLO_IR_HLO_OPS_H
#define MLIR_HLO_DIALECT_MHLO_IR_HLO_OPS_H

#include "llvm/ADT/StringRef.h"
#include "mlir/Dialect/Shape/IR/Shape.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

// clang-format off
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops_base.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops_base_attrs.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops_base_enums.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops_base_structs.h"
#include "mlir-hlo/Dialect/mhlo/IR/infer_shape_equality_op_interface.h"
// clang-format on

namespace mlir {
class OpBuilder;

namespace mhlo {

class MhloDialect : public Dialect {
 public:
  explicit MhloDialect(MLIRContext *context);
  static StringRef getDialectNamespace() { return "mhlo"; }

  // Registered hook to materialize a constant operation from a given attribute
  // value with the desired resultant type.
  Operation *materializeConstant(OpBuilder &builder, Attribute value, Type type,
                                 Location loc) override;

  // Registered hook to verify region arg attributes on operations.
  LogicalResult verifyRegionArgAttribute(mlir::Operation *op,
                                         unsigned region_index,
                                         unsigned arg_index,
                                         mlir::NamedAttribute attr) override;

  // Registered hook to verify an attribute from this dialect on operations.
  LogicalResult verifyOperationAttribute(mlir::Operation *op,
                                         mlir::NamedAttribute attr) override;

  // Parses a type registered to this dialect.
  Type parseType(DialectAsmParser &parser) const override;

  // Prints a type registered to this dialect.
  void printType(Type type, DialectAsmPrinter &os) const override;

  // Parses an attribute registered to this dialect.
  Attribute parseAttribute(DialectAsmParser &parser, Type type) const override;

  // Prints an attribute registered to this dialect.
  void printAttribute(Attribute attr, DialectAsmPrinter &os) const override;
};

class TokenType : public Type::TypeBase<TokenType, Type, TypeStorage> {
 public:
  using Base::Base;
};

// Shape derivation function that computes the shape of the result based on an
// operand. For a 2-dimensional input tensor, this produces IR of the form
//
//  %0 = dim %arg0, 0 : memref<?x?xf32>
//  %1 = index_cast %0 : index to i64
//  %2 = dim %arg0, 1 : memref<?x?xf32>
//  %3 = index_cast %2 : index to i64
//  %4 = "mhlo.scalars_to_dimension_tensor"(%1, %3)
//    : (i64, i64) -> tensor<2xi64>
//
// and returns %4 as the shape value.
LogicalResult deriveShapeFromOperand(
    OpBuilder *builder, Operation *op, Value operand,
    SmallVectorImpl<Value> *reifiedReturnShapes);

// Type derivation function that returns a tensor type with a new element type.
TensorType getSameShapeTensorType(TensorType tensor_type, Type element_type);

}  // end namespace mhlo
}  // end namespace mlir

#define GET_OP_CLASSES
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h.inc"

#endif  // MLIR_HLO_DIALECT_MHLO_IR_HLO_OPS_H
