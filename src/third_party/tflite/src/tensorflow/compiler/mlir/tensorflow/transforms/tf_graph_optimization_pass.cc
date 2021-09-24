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

#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_graph_optimization_pass.h"

#include "llvm/Support/CommandLine.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Identifier.h"  // from @llvm-project
#include "mlir/IR/Location.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/export_graphdef.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/common_runtime/optimization_registry.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/protobuf/graph_debug_info.pb.h"
#include "tensorflow/core/public/session_options.h"
#include "tensorflow/stream_executor/lib/statusor.h"

#define DEBUG_TYPE "run-tf-graph-optimization"

namespace tensorflow {

// Creates a pass to convert MLIR to Graph, run user-specified Graph
// Optimization Passes and convert back to MLIR.
// Constraints: This pass expects that all operations in the MLIR module either
// belong to 'tf' or '_tf' dialect. The output is in '_tf' dialect.
class GraphOptPass
    : public mlir::PassWrapper<GraphOptPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
  void getDependentDialects(mlir::DialectRegistry& registry) const override {
    mlir::RegisterAllTensorFlowDialects(registry);
  }

 public:
  explicit GraphOptPass(std::vector<tensorflow::GraphOptimizationPass*> passes)
      : passes_(std::move(passes)) {}

 protected:
  void runOnOperation() override;

  // The passes to run on the module.
  std::vector<GraphOptimizationPass*> passes_;
};

void GraphOptPass::runOnOperation() {
  mlir::ModuleOp module_in = getOperation();
  mlir::MLIRContext& ctx = getContext();

  // Convert MLIR to Graph
  FunctionLibraryDefinition flib_def(OpRegistry::Global(),
                                     FunctionDefLibrary());
  GraphExportConfig confs;
  auto graph = absl::make_unique<Graph>(flib_def);
  Status status = ConvertMlirToGraph(module_in, confs, &graph, &flib_def);
  if (!status.ok()) {
    mlir::emitError(mlir::UnknownLoc::get(&ctx)) << status.error_message();
    return signalPassFailure();
  }

  // Run each of the passes that were selected.
  GraphConstructorOptions opts;
  opts.allow_internal_ops = true;
  opts.expect_device_spec = false;

  GraphOptimizationPassOptions options;
  SessionOptions sess_options;
  options.graph = &graph;
  options.flib_def = &flib_def;
  options.session_options = &sess_options;

  for (auto pass : passes_) {
    assert(pass != nullptr);
    Status status = pass->Run(options);
    if (!status.ok()) {
      mlir::emitError(mlir::UnknownLoc::get(&ctx))
          << pass->name() << ": " << status.error_message();
      return signalPassFailure();
    }
  }

  // Convert Graph to MLIR
  GraphDebugInfo debug_info;
  GraphImportConfig specs;
  auto module_or_status =
      ConvertGraphToMlir(**options.graph, debug_info, flib_def, specs, &ctx);
  if (!module_or_status.ok()) {
    mlir::emitError(mlir::UnknownLoc::get(&ctx))
        << module_or_status.status().error_message();
    return signalPassFailure();
  }
  auto module_out = std::move(module_or_status).ValueOrDie();

  // We cannot replace the module in a ModulePass. So we simply copy the
  // operation list from module_out to module_in.
  auto& module_in_ops = module_in.getBody()->getOperations();
  module_in_ops.clear();
  module_in_ops.splice(module_in_ops.end(),
                       module_out->getBody()->getOperations());
}

// Returns a vector of passes from their names. If a pass is not found, then the
// corresponding return entry is null.
static std::vector<GraphOptimizationPass*> FindRegisteredPassesByName(
    const std::vector<std::string>& pass_names) {
  std::vector<GraphOptimizationPass*> pass_ids(pass_names.size(), nullptr);

  for (const auto& group : OptimizationPassRegistry::Global()->groups()) {
    for (const auto& phase : group.second) {
      for (const auto& pass : phase.second) {
        // Iterate over the pass_names_ and insert the pass pointer at all the
        // corresponding indices in the pass_ids vector.
        auto iter = pass_names.begin();
        while ((iter = std::find(iter, pass_names.end(), pass->name())) !=
               pass_names.end()) {
          pass_ids[std::distance(pass_names.begin(), iter)] = pass.get();
          iter++;
        }
      }
    }
  }
  return pass_ids;
}

// TODO(prakalps): Move these flags and pass registration to a header file so
// that it is clear that this is a generic pass library and command line is used
// for testing only.

// NOLINTNEXTLINE
static llvm::cl::OptionCategory clOptionsCategory(DEBUG_TYPE " options");

// NOLINTNEXTLINE
static llvm::cl::list<std::string> cl_pass_list(
    "graph-passes", llvm::cl::value_desc("list"),
    llvm::cl::desc("comma separated list of GraphOptimizationPass to run."),
    llvm::cl::CommaSeparated, llvm::cl::cat(clOptionsCategory));

class GraphOptByNamePass : public GraphOptPass {
 public:
  explicit GraphOptByNamePass() : GraphOptByNamePass(cl_pass_list) {}
  explicit GraphOptByNamePass(const std::vector<std::string>& pass_names)
      : GraphOptPass(FindRegisteredPassesByName(pass_names)) {}

  llvm::StringRef getArgument() const final {
    return "run-tf-graph-optimization";
  }

  llvm::StringRef getDescription() const final {
    return "runs passes registered as tensorflow::GraphOptimizationPass";
  }

 private:
  void runOnOperation() override {
    // Verify all passes requested were registered/found.
    for (auto pass_it : llvm::enumerate(passes_)) {
      if (pass_it.value() == nullptr) {
        mlir::emitError(mlir::UnknownLoc::get(&getContext()))
            << "could not find pass " << cl_pass_list[pass_it.index()];
        return signalPassFailure();
      }
    }
    return GraphOptPass::runOnOperation();
  }
};

}  // namespace tensorflow

std::unique_ptr<mlir::OperationPass<mlir::ModuleOp>>
tensorflow::CreateTensorFlowGraphOptimizationPass(
    std::vector<tensorflow::GraphOptimizationPass*> tf_passes) {
  return std::make_unique<GraphOptPass>(std::move(tf_passes));
}

std::unique_ptr<mlir::OperationPass<mlir::ModuleOp>>
tensorflow::CreateTensorFlowGraphOptimizationPass(
    const std::vector<std::string>& pass_names) {
  return std::make_unique<GraphOptByNamePass>(pass_names);
}

static mlir::PassRegistration<tensorflow::GraphOptByNamePass> pass;
