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

// This pass forms `tf_executor.island` per replica from a single
// `tf_device.replicate` island.

#include <memory>
#include <utility>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Block.h"  // from @llvm-project
#include "mlir/IR/BlockAndValueMapping.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Diagnostics.h"  // from @llvm-project
#include "mlir/IR/Dialect.h"  // from @llvm-project
#include "mlir/IR/Visitors.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_executor.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/device_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/tpu_rewrite_device_util.h"

namespace mlir {
namespace TFDevice {
namespace {
constexpr char kDeviceAttr[] = "device";
constexpr char kReplicaIdAttr[] = "_xla_replica_id";
constexpr char kDeviceOrdinalAttr[] = "device_ordinal";
constexpr char kTPUCore0[] = "TPU_REPLICATED_CORE_0";

struct ReplicateToIslandPass
    : public PassWrapper<ReplicateToIslandPass, FunctionPass> {
  StringRef getArgument() const final { return "tf-replicate-to-island"; }

  StringRef getDescription() const final {
    return "Lowers device replicate to executor islands";
  }

  void runOnFunction() override;
};

// Returns whether op requires `_xla_replica_id` attribute.
bool RequiresReplicaIDAttribute(Operation* op) {
  return llvm::isa<TF::EnqueueTPUEmbeddingSparseTensorBatchOp,
                   TF::EnqueueTPUEmbeddingRaggedTensorBatchOp>(op);
}

// Collects TPU device ordinal for outside compilation communication ops. This
// currently assumes outside compilation only uses `TPU_REPLICATED_CORE_0`
// aliased device for the device computation.
llvm::Optional<int64_t> GetDeviceOrdinal(
    const llvm::Optional<DictionaryAttr>& devices, Location loc,
    unsigned replica_id) {
  int64_t device_ordinal = 0;
  if (devices.hasValue()) {
    if (auto tpu_replica_0 = devices.getValue().get(kTPUCore0)) {
      llvm::StringRef tpu_device = tpu_replica_0.cast<ArrayAttr>()[replica_id]
                                       .cast<StringAttr>()
                                       .getValue();
      if (succeeded(tensorflow::GetDeviceOrdinalFromDeviceString(
              loc, tpu_device, &device_ordinal))) {
        return llvm::Optional<int64_t>(device_ordinal);
      }
    }
  }
  return llvm::None;
}

// Updates replica variant ops in a region based on replica `replica_id`.
// TODO(b/157624749): Replace this with better abstraction to differentiate ops
// for different replicas. Some ops, such as XlaHostCompute op or TPU Embedding
// ops, require replica id to be added as an op attribute to be used during
// execution. Handle such ops separately and add an integer attribute that
// represents replica id.
LogicalResult UpdateRegionReplicateVariantOps(
    OpBuilder& builder, Location loc, Region& region, int replica_id,
    const llvm::Optional<DictionaryAttr>& devices) {
  llvm::Optional<int64_t> device_ordinal =
      GetDeviceOrdinal(devices, loc, replica_id);

  auto result = region.walk([&](Operation* op) -> WalkResult {
    if (RequiresReplicaIDAttribute(op)) {
      op->setAttr(kReplicaIdAttr, builder.getI64IntegerAttr(replica_id));
      return WalkResult::advance();
    }

    if (isa<TF::_TPUDeviceOrdinalPlaceholderOp>(op)) {
      if (!device_ordinal.hasValue())
        return op->emitOpError()
               << "requires device ordinal from device " << kTPUCore0
               << " to be present in 'tf.device.replicate' op";

      OpBuilder builder(op);
      auto const_op = builder.create<TF::ConstOp>(
          op->getLoc(), DenseIntElementsAttr::get(
                            RankedTensorType::get({}, builder.getI64Type()),
                            {device_ordinal.getValue()}));
      op->replaceAllUsesWith(const_op);
      op->erase();
      return WalkResult::advance();
    }

    if (!devices.hasValue()) return WalkResult::advance();

    // Map aliased devices to explicit devices based on replica.
    if (auto launch = dyn_cast<tf_device::LaunchOp>(op))
      if (auto device_by_replica = devices.getValue().get(launch.device()))
        launch->setAttr(
            kDeviceAttr,
            device_by_replica.cast<ArrayAttr>()[replica_id].cast<StringAttr>());

    return WalkResult::advance();
  });

  return failure(result.wasInterrupted());
}

// Creates islands per replica from `tf_device.replicate` region. If for a
// `tf_device.launch` op the device is an aliased device of the
// `tf_device.replicate`, the device will be remapped to an explicit device
// for the associated replica island.
LogicalResult ExpandReplicateIntoReplicas(
    const Dialect* tf_dialect, OpBuilder& builder,
    tf_executor::IslandOp island_op, tf_device::ReplicateOp replicate_op,
    int num_replicas, llvm::SmallVectorImpl<tf_executor::IslandOp>& replicas) {
  replicas.reserve(num_replicas);
  auto devices = replicate_op.devices();

  // Collect result types and operands.
  Operation& terminator = replicate_op.GetBody().back();
  llvm::SmallVector<Type, 8> output_types(terminator.getOperandTypes());
  auto control_type = tf_executor::ControlType::get(island_op.getContext());
  llvm::SmallVector<Value, 8> replica_inputs(island_op.controlInputs());

  // Replace replicate terminator with YieldOp.
  builder.setInsertionPoint(&terminator);
  builder.create<tf_executor::YieldOp>(terminator.getLoc(),
                                       terminator.getOperands());
  terminator.erase();

  builder.setInsertionPoint(island_op);
  BlockAndValueMapping mapping;
  for (int i : llvm::seq<int>(0, num_replicas)) {
    // Create new island for replica.
    auto replica = builder.create<tf_executor::IslandOp>(
        island_op.getLoc(), output_types, control_type, replica_inputs);

    // Map block arg to replica arg.
    mapping.clear();
    for (auto& block_arg : replicate_op.GetBody().getArguments())
      mapping.map(block_arg,
                  replicate_op.GetReplicaOperandForBlockArgument(block_arg, i));

    // Copy over replicate region into replica island.
    replicate_op.body().cloneInto(&replica.body(), mapping);

    if (failed(UpdateRegionReplicateVariantOps(builder, replicate_op.getLoc(),
                                               replica.body(),
                                               /*replica_id=*/i, devices)))
      return failure();

    replicas.push_back(replica);
  }

  return success();
}

// Creates islands per replica from `tf_device.replicate` region and remap
// replicate results with new island outputs. A single island is created to
// forward control dependencies if there is a control dependency output from the
// replicate island. Devices are remapped from aliased devices to explicit
// devices, for `tf_device.launch` ops.
//
// For example, the following:
//
// %0:2 = tf_executor.island(%control) {
//   %1:4 = tf_device.replicate([%arg0, %arg1] as %ri: tensor<i1>)
//              {n = 2 : i32,
//               devices = {DEVICE_ALIAS_0 = ["/DEVICE:0", "/DEVICE:1"],
//                          DEVICE_ALIAS_1 = ["/DEVICE:2", "/DEVICE:3"]}} {
//     %a = "tf_device.launch"() ( {
//       %2 = "tf.opA"(%ri) : (tensor<i1>) -> tensor<i1>
//       tf_device.return %2 : tensor<i1>
//     }) {device = "DEVICE_ALIAS_0"} : () -> tensor<i1>
//     %b = "tf_device.launch"() ( {
//       %3 = "tf.opB"(%a) : (tensor<i1>) -> tensor<i1>
//       tf_device.return %3 : tensor<i1>
//     }) {device = "DEVICE_ALIAS_1"} : () -> tensor<i1>
//     tf_device.return %a, %b : tensor<i1>, tensor<i1>
//   }
//   tf_executor.yield %1#0 : tensor<i1>
// }
//
// gets lowered to:
//
// %0:3 = tf_executor.island(%control) {
//   %a0 = "tf_device.launch"() ( {
//     %1 = "tf.opA"(%arg0) : (tensor<i1>) -> tensor<i1>
//     tf_device.return %1 : tensor<i1>
//   }) {device = "/DEVICE:0"} : () -> tensor<i1>
//   %b0 = "tf_device.launch"() ( {
//     %2 = "tf.opB"(%a0) : (tensor<i1>) -> tensor<i1>
//     tf_device.return %2 : tensor<i1>
//   }) {device = "/DEVICE:2"} : () -> tensor<i1>
//   tf_executor.yield %a0, %b0 : tensor<i1>, tensor<i1>
// }
// %3:3 = tf_executor.island(%control) {
//   %a1 = "tf_device.launch"() ( {
//     %4 = "tf.opA"(%arg1) : (tensor<i1>) -> tensor<i1>
//     tf_device.return %4 : tensor<i1>
//   }) {device = "/DEVICE:1"} : () -> tensor<i1>
//   %b1 = "tf_device.launch"() ( {
//     %5 = "tf.opB"(%a1) : (tensor<i1>) -> tensor<i1>
//     tf_device.return %5 : tensor<i1>
//   }) {device = "/DEVICE:3"} : () -> tensor<i1>
//   tf_executor.yield %a1, %b1 : tensor<i1>, tensor<i1>
// }
LogicalResult CreateIslandsFromReplicate(const Dialect* tf_dialect,
                                         tf_executor::GraphOp graph_op,
                                         tf_executor::IslandOp island_op,
                                         tf_device::ReplicateOp replicate_op) {
  OpBuilder builder(island_op);
  const int num_replicas = replicate_op.n();

  // Create islands per replica.
  llvm::SmallVector<tf_executor::IslandOp, 8> replicas;
  if (failed(ExpandReplicateIntoReplicas(tf_dialect, builder, island_op,
                                         replicate_op, num_replicas, replicas)))
    return failure();

  // Collect all replica results.
  llvm::SmallVector<Value, 8> replicas_outputs(replicate_op.getNumResults(),
                                               nullptr);
  for (auto replica_and_idx : llvm::enumerate(replicas))
    for (auto replica_result_and_idx :
         llvm::enumerate(replica_and_idx.value().outputs()))
      replicas_outputs[num_replicas * replica_result_and_idx.index() +
                       replica_and_idx.index()] =
          replica_result_and_idx.value();

  // Remap replicate results to per replica result.
  for (auto result : llvm::zip(island_op.outputs(), replicas_outputs))
    std::get<0>(result).replaceAllUsesWith(std::get<1>(result));

  // Add sink island to pin all replicas as a control dependency if there is a
  // control dependency leading from the replicate originally.
  if (!island_op.control().use_empty()) {
    llvm::SmallVector<Value, 8> island_operands;
    for (auto& replica : replicas) island_operands.push_back(replica.control());

    builder.setInsertionPoint(island_op);
    auto island_sink = builder.create<tf_executor::IslandOp>(
        island_op.getLoc(), llvm::ArrayRef<Type>{},
        tf_executor::ControlType::get(island_op.getContext()), island_operands);
    island_sink.body().push_back(new Block);
    builder.setInsertionPointToEnd(&island_sink.GetBody());
    builder.create<tf_executor::YieldOp>(island_op.getLoc(),
                                         llvm::ArrayRef<Value>{});
    island_op.control().replaceAllUsesWith(island_sink.control());
  }

  // Replicas with no uses should be pinned to a graph fetch so they still
  // execute.
  llvm::SmallVector<Value, 8> unused_replica_controls;
  for (auto& replica : replicas)
    if (replica.use_empty())
      unused_replica_controls.push_back(replica.control());

  if (!unused_replica_controls.empty()) {
    tf_executor::FetchOp fetch = graph_op.GetFetch();
    auto fetches = llvm::to_vector<8>(fetch.getOperands());
    fetches.append(unused_replica_controls.begin(),
                   unused_replica_controls.end());
    builder.setInsertionPoint(fetch);
    builder.create<tf_executor::FetchOp>(fetch.getLoc(), fetches);
    fetch.erase();
  }

  island_op.erase();
  return success();
}

void ReplicateToIslandPass::runOnFunction() {
  const Dialect* tf_dialect = getContext().getLoadedDialect("tf");
  if (!tf_dialect) {
    getOperation().emitError() << "'tf' dialect is not registered";
    return signalPassFailure();
  }

  // Find islands with a single `tf_device.replicate` and create individual
  // islands per replica of the replicate.
  llvm::SmallVector<tf_executor::IslandOp, 4> replicate_op_islands;
  getOperation().walk([&](tf_executor::GraphOp graph_op) {
    for (auto island_op : graph_op.getOps<tf_executor::IslandOp>()) {
      if (!island_op.WrapsSingleOp()) continue;

      if (isa<tf_device::ReplicateOp>(&island_op.GetBody().front()))
        replicate_op_islands.push_back(island_op);
    }
  });

  for (tf_executor::IslandOp island_op : replicate_op_islands) {
    auto graph_op = island_op->getParentOfType<tf_executor::GraphOp>();
    auto replicate_op =
        cast<tf_device::ReplicateOp>(island_op.GetBody().front());
    if (failed(CreateIslandsFromReplicate(tf_dialect, graph_op, island_op,
                                          replicate_op)))
      return signalPassFailure();
  }
}
}  // anonymous namespace

std::unique_ptr<OperationPass<FuncOp>> CreateReplicateToIslandPass() {
  return std::make_unique<ReplicateToIslandPass>();
}

static PassRegistration<ReplicateToIslandPass> pass;

}  // namespace TFDevice
}  // namespace mlir
