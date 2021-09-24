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

#include "tensorflow/core/common_runtime/function_optimization_registry.h"

#include "tensorflow/core/framework/metrics.h"

namespace tensorflow {

void FunctionOptimizationPassRegistry::Init(
    std::unique_ptr<FunctionOptimizationPass> pass) {
  DCHECK(!pass_) << "Only one pass should be set.";
  pass_ = std::move(pass);
}

Status FunctionOptimizationPassRegistry::Run(
    const DeviceSet& device_set, const ConfigProto& config_proto,
    std::unique_ptr<Graph>* graph, FunctionLibraryDefinition* flib_def,
    std::vector<std::string>* control_ret_node_names,
    bool* control_rets_updated) {
  if (!pass_) return Status::OK();

  const uint64 pass_start_us = Env::Default()->NowMicros();
  Status status = pass_->Run(device_set, config_proto, graph, flib_def,
                             control_ret_node_names, control_rets_updated);
  const uint64 pass_end_us = Env::Default()->NowMicros();
  metrics::UpdateGraphOptimizationPassTime("FunctionOptimizationPassRegistry",
                                           pass_end_us - pass_start_us);
  return status;
}

// static
FunctionOptimizationPassRegistry& FunctionOptimizationPassRegistry::Global() {
  static FunctionOptimizationPassRegistry* kGlobalRegistry =
      new FunctionOptimizationPassRegistry;
  return *kGlobalRegistry;
}

}  // namespace tensorflow
