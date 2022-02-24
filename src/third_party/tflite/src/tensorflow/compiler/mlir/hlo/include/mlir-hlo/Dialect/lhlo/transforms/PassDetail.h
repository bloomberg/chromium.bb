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

#ifndef MLIR_HLO_DIALECT_LHLO_TRANSFORMS_PASSDETAIL_H
#define MLIR_HLO_DIALECT_LHLO_TRANSFORMS_PASSDETAIL_H

#include "mlir/Pass/Pass.h"

namespace mlir {
namespace scf {
class SCFDialect;
}  // namespace scf
namespace memref {
class MemRefDialect;
}  // namespace memref

namespace lmhlo {

#define GEN_PASS_CLASSES
#include "mlir-hlo/Dialect/lhlo/transforms/lmhlo_passes.h.inc"

}  // end namespace lmhlo

}  // end namespace mlir

#endif  // MLIR_HLO_DIALECT_LHLO_TRANSFORMS_PASSDETAIL_H
