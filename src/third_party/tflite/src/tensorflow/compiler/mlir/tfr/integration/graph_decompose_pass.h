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
#ifndef TENSORFLOW_COMPILER_MLIR_TFR_INTEGRATION_GRAPH_DECOMPOSE_PASS_H_
#define TENSORFLOW_COMPILER_MLIR_TFR_INTEGRATION_GRAPH_DECOMPOSE_PASS_H_

#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/mlir_graph_optimization_pass.h"
#include "tensorflow/compiler/mlir/tfr/integration/tfr_decompose_ctx.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace tensorflow {
namespace tfr {

// An optimization pass that decompose the composite ops in a module according
// to the decomposition library. Currently the decomposition library is loaded
// each time the pass runs. A special environment variable is set to locate the
// decomposition library.
class GraphDecomposePass : public MlirOptimizationPass {
 public:
  llvm::StringRef name() const override { return "tfr"; }

  // Whether to run this pass. If this is enabled, the GraphDef will be imported
  // to MLIR even no tf composition file is found.
  ::tensorflow::MlirOptimizationPassState GetPassState(
      const DeviceSet* device_set, const ConfigProto& config_proto,
      const Graph& graph,
      const FunctionLibraryDefinition& function_library) const override;

  // This should be used as a thin mapper around mlir::ModulePass::runOnModule
  // API integrated with the Tensorflow runtime.
  Status Run(const ConfigProto& config_proto, mlir::ModuleOp module,
             const Graph& graph,
             const FunctionLibraryDefinition& function_library) override;
};

}  // namespace tfr
}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFR_INTEGRATION_GRAPH_DECOMPOSE_PASS_H_
