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

#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/test_passes.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/test_passes_detail.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_saved_model_passes.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/fake_session.h"

namespace mlir {
namespace tf_test {
namespace {

struct FreezeVariableTestPass
    : public ::mlir::tf_test::FreezeVariablesTestPassBase<
          FreezeVariableTestPass> {
  void runOnOperation() override {
    TF::test_util::FakeSession session;
    PassManager pass_manager(&getContext(), "builtin.module");
    pass_manager.addPass(tf_saved_model::CreateFreezeVariablesPass(&session));
    auto status = pass_manager.run(getOperation());
    if (status.failed()) signalPassFailure();
  }
};
}  // namespace

std::unique_ptr<OperationPass<ModuleOp>> CreateFreezeVariableTestPass() {
  return std::make_unique<FreezeVariableTestPass>();
}
}  // namespace tf_test
}  // namespace mlir
