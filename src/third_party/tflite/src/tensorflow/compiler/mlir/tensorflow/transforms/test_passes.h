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
#ifndef TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSFORMS_TEST_PASSES_H_
#define TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSFORMS_TEST_PASSES_H_

#include <memory>

#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project

namespace mlir {
namespace tf_test {

// Returns test pass for variable freezing.
std::unique_ptr<OperationPass<ModuleOp>> CreateFreezeVariableTestPass();

// Test pass for applying TF->TF lowering patterns.
std::unique_ptr<OperationPass<FuncOp>> CreateTestTFLowerTFPass();

// Test passes for visitor util.
std::unique_ptr<OperationPass<FuncOp>> CreateTestVisitorUtilPass();
std::unique_ptr<OperationPass<FuncOp>> CreateTestVisitorUtilInterruptPass();

// Test operation clustering based on user defined policy.
std::unique_ptr<OperationPass<FuncOp>> CreateTestClusteringPolicyPass();

// Test pass for analyzing side-effect analysis result.
std::unique_ptr<OperationPass<ModuleOp>> CreateTestSideEffectAnalysisPass();

#define GEN_PASS_REGISTRATION
#include "tensorflow/compiler/mlir/tensorflow/transforms/test_passes.h.inc"

}  // namespace tf_test
}  // namespace mlir
#endif  // TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSFORMS_TEST_PASSES_H_
