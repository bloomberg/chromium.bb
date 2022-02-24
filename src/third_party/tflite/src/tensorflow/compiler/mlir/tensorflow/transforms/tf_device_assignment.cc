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

// This file implements device assignment in TF dialect.
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes_detail.h"

namespace mlir {
namespace TF {
namespace {

class SimpleTFDeviceAssignmentPass
    : public SimpleTFDeviceAssignmentPassBase<SimpleTFDeviceAssignmentPass> {
 public:
  SimpleTFDeviceAssignmentPass() = default;
  SimpleTFDeviceAssignmentPass(const SimpleTFDeviceAssignmentPass&) {}
  explicit SimpleTFDeviceAssignmentPass(llvm::StringRef default_device) {
    default_device_ = std::string(default_device);
  }

  void runOnOperation() override {
    Builder builder(&getContext());
    Dialect* tf = getContext().getLoadedDialect<TensorFlowDialect>();
    getOperation().walk([&](Operation* op) {
      if (auto device_attr = op->getAttrOfType<StringAttr>("device")) {
        // We assign default device to ops with device attribute that is empty.
        if (device_attr.getValue().empty()) {
          op->setAttr("device", builder.getStringAttr(default_device_));
        }
      } else if (op->getDialect() == tf) {
        // Assign default device to all ops in Tensorflow dialect that do not
        // have device attribute.
        op->setAttr("device", builder.getStringAttr(default_device_));
      }
    });
  }
};
}  // namespace

std::unique_ptr<OperationPass<FuncOp>> CreateSimpleTFDeviceAssignmentPass(
    llvm::StringRef default_device) {
  return std::make_unique<SimpleTFDeviceAssignmentPass>(default_device);
}

}  // namespace TF
}  // namespace mlir
