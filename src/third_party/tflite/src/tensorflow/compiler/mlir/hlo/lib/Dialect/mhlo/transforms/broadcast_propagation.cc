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

#include <algorithm>

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir-hlo/Dialect/mhlo/transforms/PassDetail.h"
#include "mlir-hlo/Dialect/mhlo/transforms/passes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"

namespace mlir {
namespace mhlo {
namespace {

// To avoid duplicate broadcasts, we collect all the intended broadcasts ahead
// of realizing any broadcasts in the IR. These are broadcasted versions of
// values that we are interested in, and they are uniquely characterized by a
// `BroadcastIntent` value.
struct BroadcastIntent {
  RankedTensorType result_type;
  Value target_value;
  Value output_dimensions;
  Attribute broadcast_dimensions;
  bool operator==(BroadcastIntent rhs) const {
    return result_type == rhs.result_type && target_value == rhs.target_value &&
           output_dimensions == rhs.output_dimensions &&
           broadcast_dimensions == rhs.broadcast_dimensions;
  }
  bool operator!=(BroadcastIntent rhs) const { return !(*this == rhs); }
};

}  // namespace
}  // namespace mhlo
}  // namespace mlir

namespace llvm {

template <>
struct DenseMapInfo<mlir::mhlo::BroadcastIntent> {
  static mlir::mhlo::BroadcastIntent getEmptyKey() {
    return {DenseMapInfo<mlir::RankedTensorType>::getEmptyKey(),
            DenseMapInfo<mlir::Value>::getEmptyKey(),
            DenseMapInfo<mlir::Value>::getEmptyKey(),
            DenseMapInfo<mlir::Attribute>::getEmptyKey()};
  }
  static mlir::mhlo::BroadcastIntent getTombstoneKey() {
    return {DenseMapInfo<mlir::RankedTensorType>::getTombstoneKey(),
            DenseMapInfo<mlir::Value>::getTombstoneKey(),
            DenseMapInfo<mlir::Value>::getTombstoneKey(),
            DenseMapInfo<mlir::Attribute>::getTombstoneKey()};
  }
  static unsigned getHashValue(const mlir::mhlo::BroadcastIntent &intent) {
    return hash_combine(
        DenseMapInfo<mlir::RankedTensorType>::getHashValue(intent.result_type),
        DenseMapInfo<mlir::Value>::getHashValue(intent.target_value),
        DenseMapInfo<mlir::Value>::getHashValue(intent.output_dimensions),
        DenseMapInfo<mlir::Attribute>::getHashValue(
            intent.broadcast_dimensions));
  }
  static bool isEqual(const mlir::mhlo::BroadcastIntent &lhs,
                      const mlir::mhlo::BroadcastIntent &rhs) {
    return lhs == rhs;
  }
};

}  // namespace llvm

namespace mlir {
namespace mhlo {
namespace {

bool AllowsForBroadcastPropagation(Operation *op) {
  if (op->hasTrait<mlir::OpTrait::SameOperandsAndResultShape>() &&
      op->hasTrait<mlir::OpTrait::Elementwise>() && op->getNumResults() == 1) {
    return true;
  }
  if (op->hasTrait<mlir::mhlo::OpTrait::BroadcastingElementwise>() &&
      op->getNumResults() == 1) {
    return true;
  }
  return false;
}

void TransitivelyEraseUnusedSideEffectFreeOps(Operation *root) {
  SmallPtrSet<Operation *, 16> ops_to_erase;
  SmallVector<Operation *, 16> ops_to_erase_ordered;

  // Find all the ops to erase.
  SmallVector<Operation *> worklist = {root};
  while (!worklist.empty()) {
    Operation *op = worklist.back();
    worklist.pop_back();

    // Erase ops only once.
    if (ops_to_erase.count(op)) continue;

    // Erase only operations that are unused and free of side effects.
    if (!MemoryEffectOpInterface::hasNoEffect(op) ||
        !llvm::all_of(op->getUsers(), [&](Operation *user) {
          return ops_to_erase.count(user);
        })) {
      continue;
    }

    // Erase and "recur".
    ops_to_erase.insert(op);
    ops_to_erase_ordered.push_back(op);
    for (Value operand : op->getOperands()) {
      if (Operation *def = operand.getDefiningOp()) worklist.push_back(def);
    }
  }

  // Finally, erase the ops.
  for (Operation *op : ops_to_erase_ordered) op->erase();
}

bool IsAvailableAt(Value value, Operation *op) {
  if (Operation *def = value.getDefiningOp())
    return def->getBlock() == op->getBlock() && def->isBeforeInBlock(op);
  return value.cast<BlockArgument>().getParentBlock() == op->getBlock();
}

// Assumes that all values are defined within the block or dominate the block.
void SetInsertionPointToEarliestPointWithAllValuesAvailable(OpBuilder &b,
                                                            Block *block,
                                                            ValueRange values) {
  Operation *last_def = nullptr;
  for (Value v : values) {
    Operation *def = v.getDefiningOp();
    if (def && def->getBlock() == block) {
      if (!last_def || last_def->isBeforeInBlock(def)) last_def = def;
    }
  }
  if (last_def) {
    b.setInsertionPointAfter(last_def);
  } else {
    b.setInsertionPointToStart(block);
  }
}

void PropagateBroadcast(DynamicBroadcastInDimOp root) {
  OpBuilder builder(root.getContext());
  BroadcastIntent root_bcast_intent = {
      root.getResult().getType().cast<RankedTensorType>(), root.operand(),
      root.output_dimensions(), root.broadcast_dimensions()};

  // We can move broadcasts up over (broadcasting) element-wise operations and
  // propagate them through the IR to perform them early. Instead of
  // broadcasting the result of such an op, we can broadcast the operands and
  // apply the element-wise operation to them.
  //
  // To avoid exponential growth of the IR, we will do this in two phases:
  //   1) First, we collect all the unique broadcast intents. These are
  //      broadcasted versions of values that we are interested in. They may
  //      later be materialized as an explicit broadcast or they can be the
  //      direct result of an operation over which a broadcast was propagated.
  //   2) Then, we fulfill every broadcast intent in reverse topological order
  //      to ensure that their dependencies (the broadcasted operands) are
  //      available.

  // Collect all the broadcast intents, starting with the root. Record
  // dependencies for later lookups.
  DenseSet<BroadcastIntent> bcast_intents = {root_bcast_intent};
  SmallVector<BroadcastIntent> bcast_intents_ordered = {root_bcast_intent};
  DenseMap<BroadcastIntent, SmallVector<BroadcastIntent>>
      bcast_propagation_dependencies;
  Block *the_block = root->getBlock();

  // We use the ordered broadcast intents as a worklist, the first `i` intents
  // of which have been processed.
  auto empty_broadcast_dimensions = builder.getI64TensorAttr({});
  for (int i = 0; i < bcast_intents_ordered.size(); ++i) {
    BroadcastIntent it = bcast_intents_ordered[i];
    Operation *producer_op = it.target_value.getDefiningOp();

    // We can propagate broadcasts over (broadcasting) element-wise operations
    // with the restriction that they must be in the same block as they may
    // depend on assumptions.
    if (producer_op && producer_op->getBlock() == the_block &&
        AllowsForBroadcastPropagation(producer_op)) {
      // Collect this broadcast propagation's dependencies: the broadcasted
      // versions of the operands that we will need in the second phase.
      SmallVector<BroadcastIntent> dependencies;
      for (auto operand : producer_op->getOperands()) {
        auto operand_ty = operand.getType().cast<RankedTensorType>();
        auto operand_broadcast_dimensions = operand_ty.getRank() == 0
                                                ? empty_broadcast_dimensions
                                                : it.broadcast_dimensions;
        auto bcasted_operand_ty = RankedTensorType::get(
            it.result_type.getShape(), operand_ty.getElementType());
        BroadcastIntent bcasted_operand_intent = {bcasted_operand_ty, operand,
                                                  it.output_dimensions,
                                                  operand_broadcast_dimensions};
        dependencies.push_back(bcasted_operand_intent);

        // If this broadcast intent was not yet seen, add it to the worklist.
        // Otherwise, we know it will be fulfilled earlier.
        if (!bcast_intents.count(bcasted_operand_intent)) {
          bcast_intents_ordered.push_back(bcasted_operand_intent);
          bcast_intents.insert(bcasted_operand_intent);
        }
      }
      bcast_propagation_dependencies[it] = dependencies;
    }
  }

  // Realize all the broadcast intents in reverse topological order of the
  // producer ops. We can use the positions in the block for this. All broadcast
  // intents outside the block (e.g. arguments) will be sorted towards the
  // front.
  // This ordering is independent of the output dimensions as dependencies can
  // only occur between broadcast intents of the same output dimension.
  DenseMap<BroadcastIntent, Value> realizations;
  std::sort(bcast_intents_ordered.begin(), bcast_intents_ordered.end(),
            [&](const BroadcastIntent &a, const BroadcastIntent &b) {
              Operation *producer_op_a = a.target_value.getDefiningOp();
              Operation *producer_op_b = b.target_value.getDefiningOp();
              bool a_in_block =
                  producer_op_a && producer_op_a->getBlock() == the_block;
              bool b_in_block =
                  producer_op_b && producer_op_b->getBlock() == the_block;
              if (a_in_block && b_in_block) {
                return producer_op_a->isBeforeInBlock(producer_op_b);
              }
              return !a_in_block && b_in_block;
            });
  for (auto it : bcast_intents_ordered) {
    Operation *producer_op = it.target_value.getDefiningOp();

    // Realize broadcast intent for an element-wise operation based on the
    // broadcasted operands, if possible.
    if (bcast_propagation_dependencies.count(it)) {
      assert(producer_op && producer_op->getBlock() == the_block &&
             AllowsForBroadcastPropagation(producer_op) &&
             "expect (broadcasting)  element-wise op in the same block");
      auto bcasted_operands =
          llvm::to_vector(llvm::map_range(bcast_propagation_dependencies[it],
                                          [&](BroadcastIntent operand_intent) {
                                            return realizations[operand_intent];
                                          }));
      SetInsertionPointToEarliestPointWithAllValuesAvailable(builder, the_block,
                                                             bcasted_operands);
      OperationState new_producer_op_state(
          producer_op->getLoc(), producer_op->getName().getStringRef(),
          bcasted_operands, it.result_type, producer_op->getAttrs());
      Operation *new_producer_op =
          builder.createOperation(new_producer_op_state);
      assert(new_producer_op->getNumResults() == 1 &&
             "expect exactly one result");
      realizations[it] = new_producer_op->getResults().front();
      continue;
    }

    // Fall back to explicit broadcasts, otherwise.
    SetInsertionPointToEarliestPointWithAllValuesAvailable(
        builder, the_block, ValueRange{it.target_value, it.output_dimensions});
    realizations[it] = builder.create<DynamicBroadcastInDimOp>(
        it.target_value.getLoc(), it.result_type, it.target_value,
        it.output_dimensions,
        it.broadcast_dimensions.cast<DenseIntElementsAttr>());
  }

  // Lookup the replacement for the root operation.
  auto replacement = realizations[root_bcast_intent];
  root->replaceAllUsesWith(ValueRange{replacement});

  // Erase all the operations that have become redundant as a result of this
  // rewrite.
  TransitivelyEraseUnusedSideEffectFreeOps(root);
}

struct BroadcastPropagationPass
    : public BroadcastPropagationPassBase<BroadcastPropagationPass> {
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<mhlo::MhloDialect>();
  }

  void runOnFunction() override {
    getFunction().walk(
        [&](DynamicBroadcastInDimOp bcast) { PropagateBroadcast(bcast); });
  }
};

}  // namespace

std::unique_ptr<FunctionPass> createBroadcastPropagationPass() {
  return std::make_unique<BroadcastPropagationPass>();
}

}  // namespace mhlo
}  // namespace mlir
