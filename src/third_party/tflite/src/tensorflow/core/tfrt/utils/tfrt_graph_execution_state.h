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
#ifndef TENSORFLOW_CORE_TFRT_UTILS_TFRT_GRAPH_EXECUTION_STATE_H_
#define TENSORFLOW_CORE_TFRT_UTILS_TFRT_GRAPH_EXECUTION_STATE_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/core/common_runtime/graph_execution_state.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/protobuf/config.pb.h"
#include "tensorflow/core/tfrt/fallback/fallback_state.h"
#include "tensorflow/core/tfrt/utils/statusor.h"

namespace tensorflow {
namespace tfrt_stub {

// This is a TFRT variant of `tensorflow::GraphExecutionState`. It wraps
// `tensorflow::GraphExecutionState` and adds TFRT-specific adjustments.
//
// Responsible for generating an executable `Graph` from the original `GraphDef`
// that specifies the complete graph and from `GraphImportConfig` that specifies
// input/output nodes.
//
// Thread-safe.
class TfrtGraphExecutionState {
 public:
  struct OptimizationResult {
    std::unique_ptr<tensorflow::Graph> graph;
    absl::Duration functionalization_duration;
    absl::Duration grappler_duration;
  };

  // Creates a `GraphExecutionState` given `graph_def` and `fallback_state`.
  static StatusOr<std::unique_ptr<TfrtGraphExecutionState>> Create(
      tensorflow::GraphDef graph_def, const FallbackState& fallback_state,
      bool run_placer_grappler_on_nested_functions = false);

  // Ctor. Do not use directly. Public only for `std::make_unique<>()`.
  TfrtGraphExecutionState(
      std::unique_ptr<tensorflow::GraphExecutionState> graph_execution_state,
      const FallbackState& fallback_state,
      bool run_placer_grappler_on_functions,
      absl::flat_hash_set<std::string> functions_to_optimize)
      : graph_execution_state_(std::move(graph_execution_state)),
        fallback_state_(fallback_state),
        run_placer_grappler_on_functions_(run_placer_grappler_on_functions),
        functions_to_optimize_(std::move(functions_to_optimize)) {}

  // Creates an optimized graph by pruning with `graph_import_config` and
  // best-effort Grappler run.
  StatusOr<OptimizationResult> CreateOptimizedGraph(
      const tensorflow::GraphImportConfig& graph_import_config);

 private:
  // Return the preprocessed full graph. Note that it does not contain the
  // function library in the original graph.
  const tensorflow::Graph& graph() const {
    DCHECK(graph_execution_state_->full_graph());
    return *graph_execution_state_->full_graph();
  }

  // Return the function library in the original graph.
  const FunctionLibraryDefinition& flib_def() const {
    return graph_execution_state_->flib_def();
  }

  StatusOr<std::unique_ptr<tensorflow::Graph>> OptimizeGraph(
      const tensorflow::Graph& graph,
      const tensorflow::BuildGraphOptions& build_graph_options);

  std::unique_ptr<tensorflow::GraphExecutionState> graph_execution_state_;
  const FallbackState& fallback_state_;
  bool run_placer_grappler_on_functions_;
  // Only valid if `run_placer_grappler_on_functions_` is true.
  absl::flat_hash_set<std::string> functions_to_optimize_;
};

// Prunes the `graph_def` using the feed/fetch nodes specified in
// `callable_options`. It is a TFRT-specific version that it performs more
// pruning (e.g., prunes the input edges to the feed nodes) than
// `ComputeTransitiveFanin()` so that the graph can be functionalized properly
// later.
Status PruneGraphDef(GraphDef& graph_def,
                     const CallableOptions& callable_options);

// Eliminates ref variables in V1 control flow, which is required for
// functionalization. Current strategy is to insert an identity node between
// each ref node and its ref input and in-place update the ref node to its
// non-ref counterpart.
Status EliminateRefVariablesFromV1ControlFlow(GraphDef& graph_def);

}  // namespace tfrt_stub
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_TFRT_UTILS_TFRT_GRAPH_EXECUTION_STATE_H_
