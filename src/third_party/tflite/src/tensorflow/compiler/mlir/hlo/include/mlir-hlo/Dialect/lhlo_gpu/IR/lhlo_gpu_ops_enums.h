/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

// This file defines enums used in the LMHLO_GPU dialect.

#ifndef MLIR_HLO_DIALECT_LHLO_GPU_IR_LHLO_GPU_OPS_ENUMS_H
#define MLIR_HLO_DIALECT_LHLO_GPU_IR_LHLO_GPU_OPS_ENUMS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/TypeSwitch.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"

// Order matters, this .inc header is not self-contained, and relies on the
// #includes above.
#include "mlir-hlo/Dialect/lhlo_gpu/IR/lhlo_gpu_ops_enums.h.inc"
#define GET_ATTRDEF_CLASSES
#include "mlir-hlo/Dialect/lhlo_gpu/IR/lhlo_gpu_ops_attrdefs.h.inc"

#endif  // MLIR_HLO_DIALECT_LHLO_GPU_IR_LHLO_GPU_OPS_ENUMS_H
