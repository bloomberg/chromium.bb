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

#include <memory>

#include "mlir-hlo/Dialect/gml_st/IR/gml_st_ops.h"
#include "mlir-hlo/Dialect/gml_st/transforms/pass_detail.h"
#include "mlir-hlo/Dialect/gml_st/transforms/passes.h"
#include "mlir-hlo/Dialect/gml_st/transforms/tiling_interface.h"
#include "mlir/Pass/Pass.h"

namespace mlir {
namespace gml_st {
namespace {

class GreedyTilingPass : public GreedyTilingPassBase<GreedyTilingPass> {
  void runOnOperation() final {
    getOperation()->walk([](MaterializeOp materialize) {
      auto *defining_op = materialize.source().getDefiningOp();
      if (auto tileable = dyn_cast_or_null<TilingInterface>(defining_op)) {
        OpBuilder builder(materialize);
        if (auto newTile = tileable.tile(materialize, builder)) {
          materialize.replaceAllUsesWith(newTile);
          materialize.erase();
        }
      }
    });
  }

  void getDependentDialects(DialectRegistry &registry) const final {
    registry.insert<GmlStDialect>();
    registerTilingInterfaceExternalModels(registry);
  }
};

}  // namespace

std::unique_ptr<OperationPass<func::FuncOp>> createGreedyTilingPass() {
  return std::make_unique<GreedyTilingPass>();
}

}  // namespace gml_st
}  // namespace mlir
