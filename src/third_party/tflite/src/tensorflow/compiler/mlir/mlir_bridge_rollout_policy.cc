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

#include "tensorflow/compiler/mlir/mlir_bridge_rollout_policy.h"

#include "tensorflow/compiler/jit/flags.h"

namespace tensorflow {

MlirBridgeRolloutPolicy GetMlirBridgeRolloutPolicy(
    const tensorflow::Graph& graph,
    const FunctionLibraryDefinition* function_library,
    absl::optional<ConfigProto> config_proto,
    bool uses_uninitialized_resource_args, bool record_stats) {
  switch (GetMlirBridgeRolloutState(config_proto)) {
    case ConfigProto::Experimental::MLIR_BRIDGE_ROLLOUT_ENABLED:
      return MlirBridgeRolloutPolicy::kEnabledByUser;
    case ConfigProto::Experimental::MLIR_BRIDGE_ROLLOUT_DISABLED:
      return MlirBridgeRolloutPolicy::kDisabledByUser;
    default:
      // User did not explicitly enable or disable the bridge. For now, disable
      // the bridge.
      return MlirBridgeRolloutPolicy::kDisabledAfterGraphAnalysis;
  }
}

}  // namespace tensorflow
