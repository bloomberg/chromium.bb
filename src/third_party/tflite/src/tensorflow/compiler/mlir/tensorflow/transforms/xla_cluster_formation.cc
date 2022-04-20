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

// This pass encapsulates StatefulPartitionedCallOp within a Cluster op.

#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_device_passes_detail.h"

namespace mlir {

namespace {
// XlaClusterFormation Pass will encapsulate StatefulPartitionedCallOp.
struct XlaClusterFormationPass
    : public TFDevice::XlaClusterFormationPassBase<XlaClusterFormationPass> {
  void runOnOperation() override;
};

void EncapsulateStatefulpartitionedCall(TF::StatefulPartitionedCallOp call_op) {
  mlir::OpBuilder builder(call_op);

  auto cluster = builder.create<mlir::tf_device::ClusterOp>(
      call_op.getLoc(), call_op.getResultTypes());

  call_op.replaceAllUsesWith(cluster.getResults());

  cluster.body().push_back(new mlir::Block);

  call_op.getOperation()->moveBefore(&cluster.GetBody(),
                                     cluster.GetBody().end());

  builder.setInsertionPointToEnd(&cluster.GetBody());
  builder.create<mlir::tf_device::ReturnOp>(call_op.getLoc(),
                                            call_op->getResults());
}

// Encapsulate StatefulpartitionedCall op by outline the op. Inline the ops
// within a cluster.
void XlaClusterFormationPass::runOnOperation() {
  ModuleOp module = getOperation();

  llvm::SmallVector<TF::StatefulPartitionedCallOp, 4> ops;
  module.walk(
      [&](TF::StatefulPartitionedCallOp call_op) { ops.push_back(call_op); });

  for (auto call_op : ops) {
    EncapsulateStatefulpartitionedCall(call_op);
  }
}

}  // namespace

namespace TFDevice {
std::unique_ptr<OperationPass<ModuleOp>> CreateXlaClusterFormationPass() {
  return std::make_unique<XlaClusterFormationPass>();
}
}  // namespace TFDevice

}  // namespace mlir
