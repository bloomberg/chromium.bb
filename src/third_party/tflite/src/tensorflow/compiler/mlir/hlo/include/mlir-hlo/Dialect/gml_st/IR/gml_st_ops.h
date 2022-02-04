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

// This file defines the operations used in the GML ST dialect.

#ifndef TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_GML_ST_IR_GML_ST_OPS_H_
#define TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_GML_ST_IR_GML_ST_OPS_H_

#include "mlir/IR/Dialect.h"
#include "mlir/IR/MLIRContext.h"

namespace mlir {
namespace gml_st {

class GmlStDialect : public Dialect {
 public:
  explicit GmlStDialect(MLIRContext *context);
  static StringRef getDialectNamespace() { return "gml_st"; }
};

}  // namespace gml_st
}  // namespace mlir

#define GET_OP_CLASSES
#include "mlir-hlo/Dialect/gml_st/IR/gml_st_ops.h.inc"

#endif  //  TENSORFLOW_COMPILER_MLIR_HLO_INCLUDE_MLIR_HLO_DIALECT_GML_ST_IR_GML_ST_OPS_H_
