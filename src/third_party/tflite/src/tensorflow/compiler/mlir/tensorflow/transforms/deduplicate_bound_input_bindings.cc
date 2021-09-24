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

#include <vector>

#include "llvm/ADT/DenseMap.h"
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_saved_model.h"

namespace mlir {
namespace tf_saved_model {
namespace {

class DedupBoundInputBindingPass
    : public PassWrapper<DedupBoundInputBindingPass, FunctionPass> {
 public:
  StringRef getArgument() const final {
    return "tf-saved-model-dedup-bound-input-binding-pass";
  }

  StringRef getDescription() const final {
    return "Remove duplicate 'tf_saved_model.bound_input' bindings.";
  }

  void runOnFunction() override;
};

void DedupBoundInputBindingPass::runOnFunction() {
  FuncOp func = getFunction();
  if (!mlir::tf_saved_model::IsExported(func)) return;
  llvm::SmallDenseMap<Attribute, unsigned, 8> unique_bound_inputs;
  llvm::SmallVector<unsigned, 8> arg_indices_to_erase;
  for (unsigned i = 0, e = func.getNumArguments(); i < e; i++) {
    auto attr = func.getArgAttrOfType<FlatSymbolRefAttr>(
        i, "tf_saved_model.bound_input");
    if (!attr) continue;
    auto inserted = unique_bound_inputs.insert(std::make_pair(attr, i));
    if (inserted.second) continue;
    auto duplicate_arg = func.getArgument(i);
    auto original_arg = func.getArgument(unique_bound_inputs[attr]);
    duplicate_arg.replaceAllUsesWith(original_arg);
    arg_indices_to_erase.push_back(i);
  }
  func.eraseArguments(arg_indices_to_erase);
}

}  // namespace

static PassRegistration<DedupBoundInputBindingPass> pass;

std::unique_ptr<OperationPass<FuncOp>> CreateDedupBoundInputBindingPass() {
  return std::make_unique<DedupBoundInputBindingPass>();
}

}  // namespace tf_saved_model
}  // namespace mlir
