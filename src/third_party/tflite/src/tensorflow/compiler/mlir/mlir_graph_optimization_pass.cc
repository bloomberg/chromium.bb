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

#include "tensorflow/compiler/mlir/mlir_graph_optimization_pass.h"

#include <memory>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_os_ostream.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"  // from @llvm-project
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/Dialect/Shape/IR/Shape.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_executor.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/export_graphdef.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/device_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/dump_mlir_util.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/framework/metrics.h"
#include "tensorflow/core/lib/monitoring/counter.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/public/session_options.h"

namespace tensorflow {

auto* mlir_function_pass_fallback_count = monitoring::Counter<1>::New(
    /* metric name */ "/tensorflow/core/mlir_function_pass_fallback_count",
    /* metric description */
    "Track success/failure of MLIR pass runs when fallback used",
    /* metric field */ "status");

auto* mlir_graph_optimization_pass_fallback_count = monitoring::Counter<1>::New(
    /* metric name */
    "/tensorflow/core/mlir_graph_optimization_pass_fallback_count",
    /* metric description */
    "Track success/failure of MLIR graph optimization pass runs when fallback "
    "used",
    /* metric field */ "status");

// The status metric field is used to record success/failure of mlir
// function/graph optimization passes.
constexpr char kSuccess[] = "kSuccess";
constexpr char kFailure[] = "kFailure";

// Graph <-> MLIR transformations outcomes (for logging)
constexpr char kGraphImportFallbackFail[] = "kGraphImportFallbackFail";
constexpr char kGraphImportFail[] = "kGraphImportFail";
constexpr char kGraphImportSuccess[] = "kGraphImportSuccess";
constexpr char kRoundTripSuccess[] = "kRoundTripSuccess";
constexpr char kRoundTripFailure[] = "kRoundTripFailure";

static inline absl::string_view StringRefToView(llvm::StringRef ref) {
  return {ref.data(), ref.size()};
}

// Dumps the MLIR module to disk.
// This require the TF_DUMP_GRAPH_PREFIX to be set to a path that exist (or can
// be created).
static void DumpModule(mlir::ModuleOp module, std::string file_prefix) {
  std::string prefix = GetDumpDirFromEnvVar();
  if (prefix.empty()) return;

  auto* env = tensorflow::Env::Default();
  auto status = env->RecursivelyCreateDir(prefix);
  if (!status.ok()) {
    LOG(WARNING) << "cannot create directory '" + prefix +
                        "': " + status.error_message();
    return;
  }

  prefix += "/" + file_prefix;
  if (!tensorflow::Env::Default()->CreateUniqueFileName(&prefix, ".mlir")) {
    LOG(WARNING) << "cannot create unique filename, won't dump MLIR module.";
    return;
  }

  std::unique_ptr<WritableFile> file_writer;
  status = env->NewWritableFile(prefix, &file_writer);
  if (!status.ok()) {
    LOG(WARNING) << "cannot open file '" + prefix +
                        "': " + status.error_message();
    return;
  }

  // Print the module to a string before writing to the file.
  std::string txt_module;
  {
    llvm::raw_string_ostream os(txt_module);
    module.print(os);
  }

  status = file_writer->Append(txt_module);
  if (!status.ok()) {
    LOG(WARNING) << "error writing to file '" + prefix +
                        "': " + status.error_message();
    return;
  }
  (void)file_writer->Close();
  VLOG(1) << "Dumped MLIR module to " << prefix;
}

MlirOptimizationPassRegistry& MlirOptimizationPassRegistry::Global() {
  static auto* global = new MlirOptimizationPassRegistry();
  return *global;
}

static void RegisterDialects(mlir::DialectRegistry& registry) {
  // clang-format off
  registry.insert<mlir::arith::ArithmeticDialect,
                  mlir::func::FuncDialect,
                  mlir::TF::TensorFlowDialect,
                  mlir::shape::ShapeDialect,
                  mlir::tf_device::TensorFlowDeviceDialect,
                  mlir::tf_executor::TensorFlowExecutorDialect>();
  // clang-format on
}

Status MlirFunctionOptimizationPass::Run(
    const DeviceSet& device_set, const ConfigProto& config_proto,
    std::unique_ptr<Graph>* graph, FunctionLibraryDefinition* flib_def,
    std::vector<std::string>* control_ret_node_names,
    bool* control_rets_updated) {
  //  overall_state equals to:
  //    Enabled if at least one pass is Enabled.
  //    Disabled if all passes are Disabled.
  //    FallbackEnabled if there are no Enabled passes and there is at least one
  //      FallbackEnabled pass.
  MlirOptimizationPassState overall_state = MlirOptimizationPassState::Disabled;

  // Cache per pass state and reuse it during pass execution.
  std::vector<MlirOptimizationPassState> per_pass_state;
  per_pass_state.reserve(registry_->passes().size());

  int num_passes_enabled = 0, num_passes_disabled = 0,
      num_passes_fallback_enabled = 0;
  for (const auto& pass_registration : registry_->passes()) {
    MlirOptimizationPassState pass_state = pass_registration.pass->GetPassState(
        &device_set, config_proto, **graph, *flib_def);
    per_pass_state.push_back(pass_state);
    switch (pass_state) {
      case MlirOptimizationPassState::FallbackEnabled: {
        if (overall_state != MlirOptimizationPassState::Enabled)
          overall_state = MlirOptimizationPassState::FallbackEnabled;
        ++num_passes_fallback_enabled;
        break;
      }
      case MlirOptimizationPassState::Enabled: {
        overall_state = MlirOptimizationPassState::Enabled;
        ++num_passes_enabled;
        break;
      }
      case MlirOptimizationPassState::Disabled: {
        ++num_passes_disabled;
        break;
      }
    }
  }

  static const char* kTfMlirCategory = "TfMlir";
  tensorflow::metrics::ScopedCounter<2> timings(
      tensorflow::metrics::GetGraphOptimizationCounter(),
      {kTfMlirCategory, "graph_analysis"});
  // Capture stats on graph properties analyzed before running the MLIR bridge.
  // We set `uses_uninitialized_resource_args` to false here because function
  // optimization is not affected by uninitialized resource args.
  GetMlirBridgeRolloutPolicy(**graph, flib_def, config_proto,
                             /*uses_uninitialized_resource_args=*/false,
                             /*record_stats=*/true);
  timings.ReportAndStop();

  if (overall_state == MlirOptimizationPassState::Disabled) {
    if (VLOG_IS_ON(1)) {
      LOG_FIRST_N(INFO, 1)
          << "None of the MLIR Optimization Passes are enabled "
          << "(registered " << registry_->passes().size() << ")";
    }
    return Status::OK();
  }

  if (VLOG_IS_ON(1)) {
    LOG_FIRST_N(INFO, 1) << "MLIR Graph Optimization Passes."
                         << " Enabled: " << num_passes_enabled
                         << ", Disabled: " << num_passes_disabled
                         << ", FallbackEnabled: " << num_passes_fallback_enabled
                         << ", Total: " << registry_->passes().size();
  }

  GraphDebugInfo debug_info;
  mlir::DialectRegistry registry;
  RegisterDialects(registry);
  mlir::MLIRContext context(registry);
  GraphImportConfig import_config;
  import_config.graph_as_function = true;
  import_config.control_outputs = *control_ret_node_names;
  import_config.upgrade_legacy = true;
  // Disable shape inference during import as some TensorFlow op fails during
  // shape inference with dynamic shaped operands. This in turn causes the
  // import to fail. Shape inference during import is going to be removed and
  // the shape inference pass is run early in the pass pipeline, shape inference
  // during import is not necessary.
  import_config.enable_shape_inference = false;

  timings.Reset({kTfMlirCategory, "convert_graph_to_mlir"});
  auto module_ref_status = ConvertGraphToMlir(**graph, debug_info, *flib_def,
                                              import_config, &context);
  timings.ReportAndStop();

  if (!module_ref_status.ok()) {
    // If at least one pass is enabled, return failure to the caller
    // immediately.
    if (overall_state == MlirOptimizationPassState::Enabled) {
      metrics::UpdateTfMlirGraphOptimizationPassStateCounter("",
                                                             kGraphImportFail);
      return module_ref_status.status();
    }

    // Do not fail, just keep the original TF graph unchanged in fallback mode.
    metrics::UpdateTfMlirGraphOptimizationPassStateCounter(
        "", kGraphImportFallbackFail);
    return Status::OK();
  }
  metrics::UpdateTfMlirGraphOptimizationPassStateCounter("",
                                                         kGraphImportSuccess);

  mlir::OwningOpRef<mlir::ModuleOp> module_ref =
      std::move(module_ref_status.ValueOrDie());
  AddDevicesToOp(*module_ref, &device_set);

  int per_pass_state_index = 0;
  for (auto& pass_registration : registry_->passes()) {
    llvm::StringRef name = pass_registration.pass->name();

    if (VLOG_IS_ON(1)) {
      DumpModule(*module_ref, llvm::formatv("mlir_{0}_before_", name));
    }

    Status pass_status = Status::OK();
    auto pass_state = per_pass_state[per_pass_state_index++];
    if (pass_state == MlirOptimizationPassState::Enabled) {
      VLOG(2) << "Run MLIR graph optimization pass: " << StringRefToView(name);
      timings.Reset({kTfMlirCategory, name.str()});
      pass_status = pass_registration.pass->Run(config_proto, *module_ref,
                                                **graph, *flib_def);
      timings.ReportAndStop();
    } else if (pass_state == MlirOptimizationPassState::FallbackEnabled) {
      VLOG(2) << "Run MLIR graph optimization pass with fallback: "
              << StringRefToView(name);
      // Make sure when the pass is FallbackEnabled, it only modifies the MLIR
      // module in case of no failures.
      auto module_ref_clone = module_ref->clone();
      timings.Reset({kTfMlirCategory, name.str() + "_fallback"});
      pass_status = pass_registration.pass->Run(config_proto, module_ref_clone,
                                                **graph, *flib_def);
      timings.ReportAndStop();

      if (pass_status.ok())
        module_ref = module_ref_clone;
      else
        module_ref_clone->destroy();
    } else {
      VLOG(2) << "MLIR graph optimization pass: " << StringRefToView(name)
              << " is disabled and will not be run.";
    }

    if (!pass_status.ok()) {
      // If pass failed and it is:
      //   FallbackEnabled - only collect metrics, do not propagate
      //     error to the caller.
      //   Enabled - return error back to the caller.
      if (pass_state == MlirOptimizationPassState::FallbackEnabled) {
        LOG(WARNING) << StringRefToView(name)
                     << " pass failed, continuing without the pass because the "
                        "pass has fallback enabled";
        mlir_function_pass_fallback_count->GetCell(kFailure)->IncrementBy(1);
      } else if (pass_state == MlirOptimizationPassState::Enabled) {
        return pass_status;
      }
    } else {
      if (pass_state == MlirOptimizationPassState::FallbackEnabled) {
        mlir_function_pass_fallback_count->GetCell(kSuccess)->IncrementBy(1);
      }
    }

    if (VLOG_IS_ON(1)) {
      DumpModule(*module_ref, llvm::formatv("mlir_{0}_after_", name));
    }
  }

  GraphExportConfig export_config;
  absl::flat_hash_set<Node*> control_ret_nodes;

  timings.Reset({kTfMlirCategory, "convert_mlir_to_graph"});
  // Some or all passes are enabled. Convert MLIR module and return back
  // resulted graph.
  Status status = ConvertMlirToGraph(*module_ref, export_config, graph,
                                     flib_def, &control_ret_nodes);
  if (!status.ok()) {
    metrics::UpdateTfMlirGraphOptimizationPassStateCounter("",
                                                           kRoundTripFailure);
    errors::AppendToMessage(&status,
                            "Error converting MLIR module back to graph");
    return status;
  }
  metrics::UpdateTfMlirGraphOptimizationPassStateCounter("", kRoundTripSuccess);

  timings.ReportAndStop();

  control_ret_node_names->clear();
  control_ret_node_names->reserve(control_ret_nodes.size());
  for (const auto* node : control_ret_nodes)
    control_ret_node_names->push_back(node->name());

  *control_rets_updated = true;

  return Status::OK();
}

MlirV1CompatOptimizationPassRegistry&
MlirV1CompatOptimizationPassRegistry::Global() {
  static auto* global = new MlirV1CompatOptimizationPassRegistry();
  return *global;
}

Status MlirV1CompatGraphOptimizationPass::Run(
    const GraphOptimizationPassOptions& options) {
  // Skip function graphs as MlirOptimizationPassRegistry_ will be used instead.
  // Skip if no underlying pass was registered.
  if (options.is_function_graph || !registry_->pass()) return Status::OK();

  auto pass = registry_->pass();
  auto pass_state =
      pass->GetPassState(options.device_set, options.session_options->config,
                         **options.graph, *options.flib_def);

  if (pass_state == MlirOptimizationPassState::Disabled) {
    LOG_FIRST_N(INFO, 1) << "MLIR V1 optimization pass is not enabled";
    return Status::OK();
  }

  LOG_FIRST_N(INFO, 1) << "Running MLIR Graph Optimization V1 Compat Pass";

  GraphDebugInfo debug_info;
  mlir::DialectRegistry registry;
  RegisterDialects(registry);
  mlir::MLIRContext context(registry);
  GraphImportConfig import_config;
  import_config.upgrade_legacy = true;
  // Restrict functionalization to TPU nodes to avoid problems in v1 session
  // runtime.
  import_config.restrict_functionalization_to_tpu_nodes = true;

  auto module_ref_status = ConvertGraphToMlir(
      **options.graph, debug_info, *options.flib_def, import_config, &context);
  if (!module_ref_status.ok()) {
    return (pass_state == MlirOptimizationPassState::Enabled)
               ? module_ref_status.status()
               : Status::OK();
  }

  mlir::OwningOpRef<mlir::ModuleOp> module_ref =
      std::move(module_ref_status.ValueOrDie());
  AddDevicesToOp(*module_ref, options.device_set);

  llvm::StringRef name = pass->name();
  VLOG(2) << "Run MLIR V1 graph optimization pass: " << StringRefToView(name);

  if (VLOG_IS_ON(1)) {
    DumpModule(*module_ref, llvm::formatv("mlir_{0}_before_", name));
  }

  Status pass_status = pass->Run(options, *module_ref);

  if (!pass_status.ok()) {
    if (pass_state == MlirOptimizationPassState::Enabled) return pass_status;

    if (pass_state == MlirOptimizationPassState::FallbackEnabled) {
      LOG(WARNING) << StringRefToView(name)
                   << " pass failed, continuing without the pass because the "
                      "pass has fallback enabled";
      mlir_graph_optimization_pass_fallback_count->GetCell(kFailure)
          ->IncrementBy(1);
      return Status::OK();
    }
  } else {
    if (pass_state == MlirOptimizationPassState::FallbackEnabled) {
      mlir_graph_optimization_pass_fallback_count->GetCell(kSuccess)
          ->IncrementBy(1);
    }
  }

  if (VLOG_IS_ON(1)) {
    DumpModule(*module_ref, llvm::formatv("mlir_{0}_after_", name));
  }

  GraphExportConfig export_config;
  TF_RETURN_WITH_CONTEXT_IF_ERROR(
      ConvertMlirToGraph(*module_ref, export_config, options.graph,
                         options.flib_def),
      "Error converting MLIR module back to graph");

  return Status::OK();
}

}  // namespace tensorflow
