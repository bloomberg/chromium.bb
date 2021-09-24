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

#include <queue>

#include "llvm/ADT/STLExtras.h"
#include "mlir/IR/SymbolTable.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/decompose_resource_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_device_passes_detail.h"

namespace mlir {
namespace TFDevice {
namespace {

constexpr char kBadDecompositionMessage[] =
    "Resource ops decomposition did not converge";
// TODO(prakalps): This can probably be reduced to much smaller number.
constexpr int kMaxIterations = 100;

// Populates `reachable_functions` with all functions that can be reached from
// device cluster ops.
void PopulateClusterReachableFunctions(
    ModuleOp module, SmallPtrSetImpl<Operation*>& reachable_functions) {
  SymbolTableCollection table;
  SymbolUserMap symbol_map(table, module);

  // Create map from caller to set of all callee(s).
  llvm::DenseMap<FuncOp, llvm::DenseSet<FuncOp>> caller_callee_map;

  // Use worklist to populate the set of reachable functions.
  std::queue<FuncOp> function_worklist;

  // Iterates over all functions within the module to (1) create caller-callee
  // map, and (2) initialize function worklist with functions referenced from
  // device cluster ops.
  for (auto func : module.getOps<FuncOp>()) {
    for (auto user : symbol_map.getUsers(func)) {
      // Populate caller-callee map.
      if (FuncOp caller = user->getParentOfType<FuncOp>())
        caller_callee_map[caller].insert(func);
      // Initialize function worklist with functions refrerenced in device
      // cluster.
      if (auto cluster = user->getParentOfType<tf_device::ClusterOp>()) {
        if (reachable_functions.insert(func).second)
          function_worklist.push(func);
      }
    }
  }

  // Uses worklist algorithm to insert all functions reachable from device
  // cluster ops.
  while (!function_worklist.empty()) {
    FuncOp caller = function_worklist.front();
    function_worklist.pop();
    for (auto callee : caller_callee_map[caller]) {
      if (reachable_functions.insert(callee).second)
        function_worklist.push(callee);
    }
  }
}

// Applies patterns locally on ops within `cluster` until convergence or
// `max_iterations` are reached. Returns failure if resource ops decomposition
// does not converge after `max_iterations`.
// TODO(prakalps): This can be useful to a lot of other passes in bridge.
// Extract out as a separate utility.
LogicalResult ApplyPatternsLocallyUntilConverged(
    Operation* op_with_regions, FrozenRewritePatternSet& patterns,
    int max_iterations) {
  bool changed = true;
  int iteration = 0;
  while (changed && (iteration++ < max_iterations)) {
    changed = false;
    auto walk_result =
        op_with_regions->walk([&patterns, &changed](Operation* operation) {
          bool op_changed;
          if (failed(applyOpPatternsAndFold(operation, patterns, &op_changed)))
            return WalkResult::interrupt();
          changed |= op_changed;
          return WalkResult::advance();
        });
    if (walk_result.wasInterrupted()) return failure();
  }
  // Return failure is `op_with_region` was modified changed in last iteration.
  return success(!changed);
}

// Applies patterns in only device clusters and functions reachable from such
// clusters. Returns failure if it fails to converge in `max_iterations`.
// TODO(prakalps): This can be useful to a lot of other passes in bridge.
// Extract out as a separate utility.
LogicalResult ApplyPatternsInClusterAndReachableFunctions(
    ModuleOp module, FrozenRewritePatternSet& patterns, int max_iterations) {
  SmallPtrSet<Operation*, 16> reachable_functions;
  PopulateClusterReachableFunctions(module, reachable_functions);

  // Apply patterns to reachable functions.
  for (Operation* op : reachable_functions) {
    assert(isa<FuncOp>(op));
    if (failed(applyPatternsAndFoldGreedily(op, patterns))) {
      return op->emitError() << kBadDecompositionMessage;
    }
  }

  // Apply patterns to device cluster ops.
  // Note: This module search for cluster ops is a bit wasteful as we could have
  // collected many cluster ops when we were populating reachable functions. But
  // we would still need to do a walk to find all clusters that do not
  // reference any function.
  for (FuncOp func : module.getOps<FuncOp>()) {
    // If we have already applied patterns to a function then we can skip
    // applying patterns to any device clusters it contains.
    if (reachable_functions.contains(func)) continue;

    auto walk_result = func.walk([&](tf_device::ClusterOp cluster) {
      // Cluster ops are not isolated from above so we cannot use
      // `applyPatternsAndFoldGreedily` utility. Instead we apply patterns
      // locally on each op within the cluster until convergence.
      if (failed(ApplyPatternsLocallyUntilConverged(cluster, patterns,
                                                    max_iterations))) {
        cluster.emitError() << kBadDecompositionMessage;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });
    if (walk_result.wasInterrupted()) return failure();
  }

  return success();
}

struct DecomposeResourceOpsPass
    : public DecomposeResourceOpsPassBase<DecomposeResourceOpsPass> {
  void runOnFunction() override {
    // Add lowering patterns to the list.
    OwningRewritePatternList patterns(&getContext());
    TF::PopulateDecomposeResourceOpsPatterns(&getContext(), &patterns);

    if (failed(
            applyPatternsAndFoldGreedily(getFunction(), std::move(patterns)))) {
      getFunction().emitError() << kBadDecompositionMessage;
      signalPassFailure();
    }
  }
};

struct DecomposeResourceOpsInClusterPass
    : public DecomposeResourceOpsInClusterPassBase<
          DecomposeResourceOpsInClusterPass> {
  void runOnOperation() override {
    // Add lowering patterns to the list.
    OwningRewritePatternList patterns(&getContext());
    TF::PopulateDecomposeResourceOpsPatterns(&getContext(), &patterns);
    FrozenRewritePatternSet frozen_patterns(std::move(patterns));

    if (failed(ApplyPatternsInClusterAndReachableFunctions(
            getOperation(), frozen_patterns, kMaxIterations)))
      signalPassFailure();
  }
};

}  // namespace

std::unique_ptr<OperationPass<FuncOp>> CreateDecomposeResourceOpsPass() {
  return std::make_unique<DecomposeResourceOpsPass>();
}

std::unique_ptr<OperationPass<ModuleOp>>
CreateDecomposeResourceOpsInClusterPass() {
  return std::make_unique<DecomposeResourceOpsInClusterPass>();
}

}  // namespace TFDevice
}  // namespace mlir

