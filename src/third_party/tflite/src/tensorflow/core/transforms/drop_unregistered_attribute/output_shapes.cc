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

#include "tensorflow/core/transforms/drop_unregistered_attribute/output_shapes.h"

#include <memory>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "tensorflow/core//ir/ops.h"

namespace mlir {
namespace tfg {

namespace {

#define GEN_PASS_CLASSES
#include "tensorflow/core/transforms/passes.h.inc"

}  // end namespace

struct DropOutputShapesAttrPass
    : DropOutputShapesAttrBase<DropOutputShapesAttrPass> {
  LogicalResult initialize(MLIRContext* context) override {
    for (auto& str : skip_) {
      skip_id.insert(StringAttr::get(context, str));
    }
    return success();
  }
  void runOnOperation() override {
    Operation* op = getOperation();
    op->walk([this](Operation* op) {
      if (!skip_id.count(op->getName().getIdentifier()))
        op->removeAttr("_output_shapes");
    });
  }

  // Set of operation types to skip over.
  DenseSet<StringAttr> skip_id;
};

}  // namespace tfg
}  // namespace mlir

std::unique_ptr<mlir::Pass> mlir::tfg::CreateDropOutputShapesAttrPass() {
  return std::make_unique<mlir::tfg::DropOutputShapesAttrPass>();
}
