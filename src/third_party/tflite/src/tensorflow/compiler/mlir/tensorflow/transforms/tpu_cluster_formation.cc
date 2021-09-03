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

// This transformation pass takes ops with the same `_tpu_replicate` attribute
// in a block and clusters them together under a `tf_device.cluster`.
// Associated TPUReplicateMetadata ops are removed and its attributes are copied
// over to the associated `tf_device.cluster`. If a cluster should be
// replicated, the associated `tf_device::LaunchOp` will be wrapped further with
// a `tf_device.replicate`. This pass also assumes ops of the same cluster do
// not have ops outside of the cluster that are both operands and results of the
// cluster. Note, this currently does not handle side effecting ops yet.

#include <iterator>
#include <memory>
#include <tuple>
#include <utility>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Identifier.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/RegionUtils.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_executor.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

namespace mlir {
namespace TFTPU {

namespace {

constexpr char kTPUReplicateAttr[] = "_tpu_replicate";
constexpr char kDeviceAttr[] = "device";
constexpr char kNameAttr[] = "name";
constexpr char kNumReplicasAttr[] = "num_replicas";
constexpr char kMirroredVariableIndicesAttr[] = "_mirrored_variable_indices";

constexpr char kBadTPUReplicateAttrMsg[] =
    "requires '_tpu_replicate' string attribute";

// Mapping for `_tpu_replicate` attribute to TPUReplicateMetadata attributes.
using MetadataMap =
    llvm::SmallDenseMap<llvm::StringRef, MutableDictionaryAttr, 8>;

// Mapping for `_tpu_replicate` attribute to ops of a cluster.
using ClusterMap = llvm::SmallDenseMap<llvm::StringRef,
                                       llvm::SmallSetVector<Operation*, 8>, 8>;

struct TPUClusterFormation
    : public PassWrapper<TPUClusterFormation, FunctionPass> {
  void runOnFunction() override;
};

// Creates a mapping from the TPUReplicateMetadata ops `_tpu_replicate`
// attribute to its attributes and removes the ops. If multiple
// TPUReplicateMetadata ops have the same `_tpu_replicate` attribute, an error
// will be returned.
LogicalResult CollectMetadata(Operation* op, MetadataMap* metadata_map) {
  auto result =
      op->walk([&](TF::TPUReplicateMetadataOp metadata_op) -> WalkResult {
        MutableDictionaryAttr attrs = metadata_op.getAttrs();

        // Missing or bad `_tpu_replicate` attribute.
        auto tpu_replicate_attr = attrs.get(kTPUReplicateAttr);
        if (!tpu_replicate_attr)
          return metadata_op.emitError() << kBadTPUReplicateAttrMsg;

        auto tpu_replicate_attr_str = tpu_replicate_attr.dyn_cast<StringAttr>();
        if (!tpu_replicate_attr_str ||
            tpu_replicate_attr_str.getValue().empty())
          return metadata_op.emitError() << kBadTPUReplicateAttrMsg;

        // Remove `name` attribute.
        attrs.remove(Identifier::get(kNameAttr, metadata_op.getContext()));

        auto it = metadata_map->try_emplace(tpu_replicate_attr_str.getValue(),
                                            std::move(attrs));

        // There are multiple TPUReplicateMetadata ops with the same
        // `_tpu_replicate` attribute.
        if (!it.second) {
          return metadata_op.emitError()
                 << "multiple TPUReplicateMetadata ops with the same '"
                 << kTPUReplicateAttr << "' attribute '"
                 << tpu_replicate_attr_str.getValue() << "' found";
        }

        metadata_op.erase();
        return WalkResult::advance();
      });

  // Return failure if the walk was interrupted.
  return failure(result.wasInterrupted());
}

// Collects and clusters ops with the same `_tpu_replicate` attribute. This will
// return an error if a `_tpu_replicate` attribute of an op is empty.
LogicalResult CollectAndGroupClusterOps(Block* block, ClusterMap* clusters) {
  for (Operation& op : *block) {
    if (auto attr = op.getAttrOfType<StringAttr>(kTPUReplicateAttr)) {
      if (attr.getValue().empty())
        return op.emitError()
               << "attribute '" << kTPUReplicateAttr << "' is empty";

      auto it = clusters->try_emplace(attr.getValue());
      it.first->getSecond().insert(&op);
    }
  }

  return success();
}

// Checks if an op should be moved after a cluster. There may be users of a
// cluster interleaved among the cluster ops.
bool ShouldMoveOpAfterCluster(
    Block* block, Operation* op,
    const llvm::SmallSetVector<Operation*, 8>& cluster_ops,
    const llvm::SmallSetVector<Operation*, 8>& preceding_users) {
  auto result = op->walk([&](Operation* op) {
    for (Value operand : op->getOperands()) {
      Operation* def = operand.getDefiningOp();
      // Operands may not have a defining op (BlockArgument) or is from a
      // different block.
      if (!def || def->getBlock() != block) continue;

      if (cluster_ops.count(def) != 0 || preceding_users.count(def) != 0) {
        // Op is a user of a cluster or another op that is a user of the
        // cluster (transitively), but is before the cluster.
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });

  return result.wasInterrupted();
}

// Collects ops that are before ops in the cluster but are users of other ops
// in the cluster. This may happen because users of individual ops in the
// cluster may be interleaved with other ops in the cluster.
llvm::SmallSetVector<Operation*, 8> CollectClusterPrecedingUsers(
    Block* block, const llvm::SmallSetVector<Operation*, 8>& cluster_ops) {
  llvm::SmallSetVector<Operation*, 8> preceding_users;

  for (Operation& op : llvm::make_range(Block::iterator(cluster_ops.front()),
                                        Block::iterator(cluster_ops.back())))
    if (cluster_ops.count(&op) == 0 &&
        ShouldMoveOpAfterCluster(block, &op, cluster_ops, preceding_users))
      preceding_users.insert(&op);

  return preceding_users;
}

// Collects results and associated types of the cluster that are used outside of
// the cluster. These results and types are used to create the clusters
// `tf_device.cluster` and associated terminator. Results that have no uses
// outside of the cluster (i.e. results of ops in the cluster are only consumed
// by other ops in the cluster) are pruned.
llvm::SmallVector<Value, 8> CollectClusterResults(
    Block* block, const llvm::SmallSetVector<Operation*, 8>& cluster_ops) {
  llvm::SmallVector<Value, 8> results;

  for (Operation* op : cluster_ops) {
    for (Value result : op->getResults()) {
      for (Operation* user : result.getUsers()) {
        // Check if user is not an op in the cluster.
        if (cluster_ops.count(block->findAncestorOpInBlock(*user)) == 0) {
          results.push_back(result);
          break;
        }
      }
    }
  }

  return results;
}

// Creates a `tf_device.cluster` to wrap cluster ops.
tf_device::ClusterOp CreateOpForCluster(Operation* last_cluster_op,
                                        llvm::ArrayRef<Value> results) {
  // `tf_device.cluster` will be placed at where the last op of the cluster is.
  OpBuilder builder(last_cluster_op);

  llvm::SmallVector<Type, 8> result_types;
  for (Value result : results) result_types.push_back(result.getType());

  auto cluster = builder.create<tf_device::ClusterOp>(last_cluster_op->getLoc(),
                                                      result_types);

  cluster.body().push_back(new Block);

  // Add terminator.
  builder.setInsertionPointToEnd(&cluster.GetBody());
  builder.create<tf_device::ReturnOp>(last_cluster_op->getLoc(), results);

  return cluster;
}

// Moves cluster ops to associated `tf_device.cluster` body.
void MoveClusterOpsToCluster(
    tf_device::ClusterOp cluster,
    const llvm::SmallSetVector<Operation*, 8>& cluster_ops) {
  MLIRContext* context = cluster.getContext();
  Operation* terminator = cluster.GetBody().getTerminator();

  for (Operation* cluster_op : cluster_ops) {
    // Remove `_tpu_replicate` and `device` attribute from ops in the cluster
    // as that information will be present in the `tf_device.cluster`.
    cluster_op->removeAttr(Identifier::get(kTPUReplicateAttr, context));
    cluster_op->removeAttr(Identifier::get(kDeviceAttr, context));
    cluster_op->moveBefore(terminator);
  }
}

// Replaces uses of cluster ops results outside of cluster with the associated
// `tf_device.cluster` results.
void UpdateClusterResultExternalUses(tf_device::ClusterOp cluster,
                                     llvm::ArrayRef<Value> results) {
  Block& cluster_block = cluster.GetBody();
  for (auto ret_vals : llvm::zip(results, cluster.getResults())) {
    Value old_ret = std::get<0>(ret_vals);
    Value new_ret = std::get<1>(ret_vals);
    for (auto& use : llvm::make_early_inc_range(old_ret.getUses()))
      if (!cluster_block.findAncestorOpInBlock(*use.getOwner()))
        use.set(new_ret);
  }
}

// Moves users of cluster that are before the cluster to after the cluster.
void MovePrecedingClusterUsers(tf_device::ClusterOp cluster,
                               llvm::ArrayRef<Operation*> preceding_users) {
  Operation* op_after_cluster = cluster.getOperation()->getNextNode();
  for (Operation* user : preceding_users) user->moveBefore(op_after_cluster);
}

// Sorts `tf.TPUReplicatedInput` ops by `index` attribute. Ops with an `index`
// of -1 are always after ops with a non negative `index`, and an arbitrary
// ordering is used as there are no dependencies on their relative ordering.
LogicalResult SortTPUReplicatedInputsByIndex(
    llvm::ArrayRef<Operation*> inputs,
    llvm::SmallVectorImpl<Operation*>* sorted_inputs) {
  const int input_size = inputs.size();
  sorted_inputs->resize(input_size, nullptr);
  int last_index = input_size - 1;

  for (Operation* input : inputs) {
    int64_t index =
        llvm::cast<TF::TPUReplicatedInputOp>(input).index().getLimitedValue();

    if (index >= input_size || index < -1)
      return input->emitError() << "'" << input->getName().getStringRef()
                                << "' index is not in range [-1, " << input_size
                                << "), got " << index;

    if (index == -1)
      (*sorted_inputs)[last_index--] = input;
    else
      (*sorted_inputs)[index] = input;
  }

  if (llvm::any_of(*sorted_inputs, [](Operation* op) { return op == nullptr; }))
    return inputs.front()->emitError()
           << "failed to sort '" << inputs.front()->getName().getStringRef()
           << "' ops, gap(s) found in indices";

  return success();
}

// Creates a `tf_device.replicate` to represent replication for the cluster, if
// necessary.
LogicalResult ReplicateCluster(tf_device::ClusterOp cluster, int num_replicas) {
  // No need to replicate.
  if (num_replicas == 1) return success();

  if (num_replicas < 1)
    return cluster.emitError() << "requires '" << kNumReplicasAttr
                               << "' int attribute to be at least 1";

  // Collect all used TPUReplicatedInput ops and sort by `index`.
  llvm::SmallSetVector<Operation*, 8> unique_replicated_input_ops;
  mlir::visitUsedValuesDefinedAbove(
      cluster.body(), cluster.body(), [&](mlir::OpOperand* operand) {
        Operation* def = operand->get().getDefiningOp();
        if (def && llvm::isa<TF::TPUReplicatedInputOp>(def))
          unique_replicated_input_ops.insert(def);
      });
  llvm::SmallVector<Operation*, 8> replicated_input_ops;
  if (failed(SortTPUReplicatedInputsByIndex(
          unique_replicated_input_ops.getArrayRef(), &replicated_input_ops)))
    return failure();

  // Indices of the replicate op's arguments that are mirrored variables.
  llvm::SmallVector<int64_t, 8> mirrored_variable_indices;

  // Check if number of operands of each used TPUReplicatedInput op matches
  // `num_replicas`. Collect all their operands and associated type for creating
  // the replicate op.
  llvm::SmallVector<std::pair<Operation::operand_range, Type>, 8>
      replicated_inputs;
  for (auto& pos_and_input : llvm::enumerate(replicated_input_ops)) {
    auto input = pos_and_input.value();
    if (input->getNumOperands() != num_replicas)
      return input->emitOpError() << "requires " << num_replicas << " operands";

    replicated_inputs.push_back(
        {input->getOperands(), input->getOperand(0).getType()});
    if (llvm::cast<TF::TPUReplicatedInputOp>(input).is_mirrored_variable())
      mirrored_variable_indices.push_back(pos_and_input.index());
  }

  // Create replicate op.
  OpBuilder builder(cluster);
  auto replicate_op = builder.create<tf_device::ReplicateOp>(
      cluster.getLoc(), num_replicas,
      llvm::SmallDenseMap<llvm::StringRef, llvm::SmallVector<StringRef, 4>>(),
      replicated_inputs, cluster.getResultTypes());
  if (!mirrored_variable_indices.empty())
    replicate_op.setAttr(kMirroredVariableIndicesAttr,
                         builder.getI64ArrayAttr(mirrored_variable_indices));

  // Replace replicated cluster results with replicate op results.
  for (auto result_and_idx : llvm::enumerate(cluster.getResults())) {
    Value result = result_and_idx.value();
    int idx = result_and_idx.index();
    for (auto& use : result.getUses()) {
      Operation* def = use.getOwner();
      if (!def || !llvm::isa<TF::TPUReplicatedOutputOp>(def))
        return cluster.emitError()
               << "requires output of " << cluster.getOperationName()
               << " to lead to a 'tf.TPUReplicatedOutput' op";

      if (def->getNumResults() != num_replicas)
        return def->emitOpError() << "requires " << num_replicas << " results";

      auto replicate_outputs = llvm::make_range(
          std::next(replicate_op.result_begin(), idx * num_replicas),
          std::next(replicate_op.result_begin(), (idx + 1) * num_replicas));
      def->replaceAllUsesWith(replicate_outputs);
    }
  }

  // Update replicated inputs with replicate op block arguments.
  for (auto input_and_block_arg :
       llvm::zip(replicated_input_ops, replicate_op.GetBody().getArguments())) {
    Operation* input = std::get<0>(input_and_block_arg);
    Value block_arg = std::get<1>(input_and_block_arg);
    mlir::replaceAllUsesInRegionWith(input->getResult(0), block_arg,
                                     cluster.body());
  }

  // Create terminator for replicate op and move `tf_device.cluster` into
  // replicate.
  builder.setInsertionPointToEnd(&replicate_op.GetBody());
  auto return_op = builder.create<tf_device::ReturnOp>(replicate_op.getLoc(),
                                                       cluster.getResults());
  cluster.getOperation()->moveBefore(return_op);

  return success();
}

// Forms clusters with ops of the same `_tpu_replicate` attribute under a block.
//
// For a given block, clusters are formed via grouping ops by `_tpu_replicate`
// attributes.
// For every cluster formed:
//   1. Find associated TPUReplicateMetadata attributes with the same
//      `_tpu_replicate` attribute.
//   2. Find users not in cluster that are interleaved between cluster ops.
//   3. Find external uses of cluster ops.
//   4. Create `tf_device.cluster` with results consisting of the external uses
//      of cluster ops determined at 3.
//   5. Move cluster ops to `tf_device.cluster` body.
//   6. Replace external uses of cluster ops uses with `tf_device.cluster`
//      results.
//   7. Move users from 2 to after the `tf_device.cluster`.
//   8. Wrap cluster (`tf_device.cluster`) in a `tf_device.replicate` if
//      attribute `num_replicas` is greater than 1.
//   9. Copy over TPUReplicateMetadata attributes to `tf_device.cluster`.
LogicalResult FormClustersInBlock(Block* block,
                                  const MetadataMap& metadata_map) {
  ClusterMap clusters;
  LogicalResult result = CollectAndGroupClusterOps(block, &clusters);
  if (failed(result)) return result;

  for (const auto& cluster_metadata_and_ops : clusters) {
    const auto& cluster_ops = cluster_metadata_and_ops.getSecond();

    auto cluster_metadata =
        metadata_map.find(cluster_metadata_and_ops.getFirst());

    // No TPUReplicateMetadata for a `_tpu_replicate` attribute.
    if (cluster_metadata == metadata_map.end()) {
      cluster_ops.front()->emitWarning()
          << "TPUReplicateMetadata for associated '" << kTPUReplicateAttr
          << "' attribute '" << cluster_metadata_and_ops.getFirst()
          << "' is missing";
      continue;
    }

    llvm::SmallSetVector<Operation*, 8> preceding_users =
        CollectClusterPrecedingUsers(block, cluster_ops);

    llvm::SmallVector<Value, 8> results =
        CollectClusterResults(block, cluster_ops);

    tf_device::ClusterOp cluster =
        CreateOpForCluster(cluster_ops.back(), results);

    MoveClusterOpsToCluster(cluster, cluster_ops);

    UpdateClusterResultExternalUses(cluster, results);

    MovePrecedingClusterUsers(cluster, preceding_users.getArrayRef());

    auto num_replicas = cluster_metadata->getSecond().get(kNumReplicasAttr);
    if (!num_replicas || !num_replicas.isa<mlir::IntegerAttr>())
      return cluster.emitError()
             << "requires '" << kNumReplicasAttr << "' int attribute";

    if (failed(ReplicateCluster(
            cluster, num_replicas.cast<mlir::IntegerAttr>().getInt())))
      return failure();

    // Copy TPUReplicateMetadata attributes to `tf_device.cluster`.
    cluster.setAttrs(cluster_metadata->second);
    // Exclude `num_replicas` as cluster should be replicated if necessary.
    cluster.removeAttr(kNumReplicasAttr);
  }

  return success();
}

void TPUClusterFormation::runOnFunction() {
  MetadataMap metadata_map;
  if (failed(CollectMetadata(getFunction(), &metadata_map)))
    return signalPassFailure();

  for (Block& block : getFunction())
    if (failed(FormClustersInBlock(&block, metadata_map)))
      return signalPassFailure();

  auto island_result = getFunction().walk([&](tf_executor::IslandOp island) {
    if (failed(FormClustersInBlock(&island.GetBody(), metadata_map)))
      return WalkResult::interrupt();

    return WalkResult::advance();
  });

  if (island_result.wasInterrupted()) return signalPassFailure();

  // Remove TPUReplicatedInput and TPUReplicatedOutput nodes.
  auto remove_result = getFunction().walk([&](Operation* op) {
    if (!llvm::isa<TF::TPUReplicatedInputOp>(op) &&
        !llvm::isa<TF::TPUReplicatedOutputOp>(op))
      return WalkResult::advance();

    // Forward operand to result. When `num_replicas` attribute is 1, no
    // `tf_device.replicate` is created and replicated (1) operands/results are
    // untouched.
    if (op->getNumOperands() == 1 && op->getNumResults() == 1)
      op->getResult(0).replaceAllUsesWith(op->getOperand(0));

    // Leftover TPUReplicatedInput/TPUReplicatedOutput that are not of
    // `num_replicas` to 1.
    if (!op->use_empty()) {
      op->emitOpError() << "expects " << op->getName().getStringRef()
                        << " to have no uses";
      return WalkResult::interrupt();
    }

    op->erase();

    return WalkResult::advance();
  });

  if (remove_result.wasInterrupted()) return signalPassFailure();
}
}  // anonymous namespace

std::unique_ptr<OperationPass<FuncOp>> CreateTPUClusterFormationPass() {
  return std::make_unique<TPUClusterFormation>();
}

static PassRegistration<TPUClusterFormation> pass(
    "tf-tpu-cluster-formation",
    "Form clusters from operations assigned to the same TPU cluster");

}  // namespace TFTPU
}  // namespace mlir
