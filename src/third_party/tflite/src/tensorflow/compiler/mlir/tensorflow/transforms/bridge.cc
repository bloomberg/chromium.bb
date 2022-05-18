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

#include "tensorflow/compiler/mlir/tensorflow/transforms/bridge.h"

#include <memory>

#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Transforms/Passes.h"  // from @llvm-project
#include "tensorflow/compiler/jit/flags.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/bridge_logger.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/dump_mlir_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/error_util.h"
#include "tensorflow/core/framework/metrics.h"
#include "tensorflow/core/platform/error_payloads.h"
#include "tensorflow/core/protobuf/core_platform_payloads.pb.h"

namespace mlir {
namespace {

// Add logger to bridge passmanager.
// Enable timing statistics per pass for the bridge passmanager.
void EnableDetailedLogging(PassManager *pm) {
  // Print the whole module after each pass, which requires disabling
  // multi-threading as well.
  pm->getContext()->disableMultithreading();
  pm->enableIRPrinting(std::make_unique<tensorflow::BridgeLoggerConfig>(
      /*print_module_scope=*/true));
  pm->enableTiming();
}
}  // namespace

namespace TFTPU {

namespace {
// Run the TF XLA Bridge based on the input pipeline, which can be either TPU
// bridge pipeline or non TPU bridge pipeline.
tensorflow::Status RunTFXLABridge(
    ModuleOp module, bool enable_logging,
    llvm::function_ref<void(OpPassManager &pm)> pipeline_builder) {
  PassManager bridge(module.getContext());
  ::tensorflow::applyTensorflowAndCLOptions(bridge);
  if (enable_logging || VLOG_IS_ON(1)) {
    tensorflow::DumpMlirOpToFile("tf_xla_bridge_before", module);
    if (VLOG_IS_ON(2)) EnableDetailedLogging(&bridge);
  }

  // Populate a passmanager with the list of passes that implement the bridge.
  pipeline_builder(bridge);

  // Add set of passes to lower back to graph (from tf_executor).
  TF::AddGraphExportLoweringPasses(bridge);

  mlir::StatusScopedDiagnosticHandler diag_handler(
      module.getContext(), /*propagate=*/false,
      /*filter_stack=*/!VLOG_IS_ON(1));

  LogicalResult result = bridge.run(module);
  (void)result;
  if (enable_logging || VLOG_IS_ON(1))
    tensorflow::DumpMlirOpToFile("tf_xla_bridge_after", module);
  return diag_handler.ConsumeStatus();
}

void CreateTPUBridgePipelineImpl(OpPassManager &pm) {
  // The following ops must be preserved regardless of reachability. Ideally,
  // all graphs should have control dependencies to enforce this but this is
  // currently not the case (see b/177478741).
  const llvm::SmallVector<std::string, 4> ops_to_preserve = {
      "tf.TPUReplicateMetadata", "tf.TPUCompilationResult",
      "tf.TPUReplicatedOutput"};
  pm.addNestedPass<func::FuncOp>(
      tf_executor::CreateTFExecutorGraphPruningPass(ops_to_preserve));
  // It is assumed at this stage there are no V1 control flow ops as Graph
  // functionalization is ran before import. Ops can be lifted out of
  // tf_executor dialect islands/graphs.
  pm.addNestedPass<func::FuncOp>(
      CreateExecutorDialectToFunctionalConversionPass());
  // Guarantee all functions have one use, which enables more exact shape
  // inference.
  pm.addPass(mlir::TF::CreateGuaranteeAllFuncsOneUsePass());
  // Run shape inference so that tf_executor/tf_device ops created later will
  // likely to inherit more concrete types.
  pm.addPass(TF::CreateTFShapeInferencePass());
  pm.addNestedPass<func::FuncOp>(
      CreateTPUReorderReplicateAndPartitionedInputsPass());
  pm.addPass(CreateTPUClusterFormationPass());
  // Run TPU cluster cleanup attributes so ops with no outside compiled
  // attribute have no host device attribute.
  pm.addPass(CreateTPUClusterCleanupAttributesPass());
  pm.addPass(CreateOutsideCompiledToHostLaunchPass());
  pm.addNestedPass<func::FuncOp>(TFDevice::CreateDeviceAttributeToLaunchPass());
  // Running canonicalizer before decomposing resource ops in cluster helps the
  // latter pass to converge faster as it does not have to spend time folding
  // away dead ops.
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());
  // Place DecomposeResourceOpsPass before TFExecutorConstantSinking pass
  // because DecomposeResourceOpsPass uses pattern rewriter which hoists
  // changed constants out of tf_device.Launch.
  pm.addPass(TFDevice::CreateDecomposeResourceOpsInClusterPass());
  // Encode this in its own scope so that func_pm is not mistakenly used
  // later on.
  {
    OpPassManager &func_pm = pm.nest<func::FuncOp>();
    func_pm.addPass(CreateTPUHostComputationExpansionPass());
    func_pm.addPass(CreateTPUUpdateEmbeddingEnqueueOpInputsPass());
  }
  // TODO(b/173622615): This should incrementally be moved down as
  // more passes support this representation and then can be removed once
  // all passes support it.
  pm.addPass(TFDevice::CreateHostLaunchToOutsideCompiledPass());

  // TODO(b/173622615): Once OutsideCompilation is represented by launch op and
  // the remaining passes including Inliner support it, remove this
  // LaunchToDeviceAttributePass. This LaunchToDeviceAttribute pass needs to
  // come before TPUClusterCleanupAttributes pass or else the device attribute
  // will be removed from launch causing an error.
  pm.addNestedPass<func::FuncOp>(TFDevice::CreateLaunchToDeviceAttributePass());

  // TODO(b/173622615): This can be removed once more passes support outside
  // compilation represented by op and conversion back to attribute is removed.
  pm.addPass(CreateOutsideCompiledToHostLaunchPass());
  // Note that the region-based control-flow produced here still contains
  // function call ops which get inlined by the subsequent inliner pass.
  pm.addPass(TF::CreateTFFunctionalControlFlowToRegions());
  pm.addPass(mlir::createInlinerPass());
  pm.addNestedPass<func::FuncOp>(
      TF::CreateDropWhileShapeInvariantInDeviceClusterPass());
  // Run another shape inference pass because resource decomposition might have
  // created new partial types. Also, after dropping `shape_invariant` attribute
  // from While/WhileRegion ops within cluster would lead to more precise
  // shapes.
  pm.addPass(TF::CreateTFShapeInferencePass());
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());
  pm.addPass(CreateTPUClusterCleanupAttributesPass());
  pm.addPass(TFDevice::CreateResourceOpLiftingPass());
  // Re-run the canonicalizer pass as some cleanup during resource op lifting
  // pass opens up some opportunities for canonicalization of cluster ops.
  // Specifically, we want to eliminate pass through results from the cluster
  // op.
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());

  // TODO(b/173622615): This should incrementally be moved down as
  // more passes support this representation and then can be removed once
  // all passes support it.
  pm.addPass(TFDevice::CreateHostLaunchToOutsideCompiledPass());
  pm.addNestedPass<func::FuncOp>(createCSEPass());
  if (tensorflow::GetMlirCommonFlags()
          ->tf_mlir_enable_merge_control_flow_pass) {
    pm.addPass(TFDevice::CreateMergeControlFlowPass());
  }

  pm.addPass(TFDevice::CreateMarkOpsForOutsideCompilationPass());
  pm.addPass(CreateTPUExtractHeadTailOutsideCompilationPass());
  pm.addPass(CreateTPUExtractOutsideCompilationPass());

  pm.addNestedPass<func::FuncOp>(TFDevice::CreateClusterConstantSinkingPass());
  pm.addPass(TF::CreateResourceDeviceInferencePass());
  pm.addPass(TFDevice::CreateClusterOutliningPass());
  pm.addPass(CreateTPUResourceReadForWritePass());
  pm.addPass(TFDevice::CreateMarkInputOutputAliasesPass());
  pm.addPass(CreateTPUShardingIdentificationPass());
  pm.addNestedPass<func::FuncOp>(
      CreateTPUResourceReadsWritesPartitioningPass());
  pm.addPass(TFDevice::CreateAnnotateParameterReplicationPass());
  pm.addPass(CreateTPURewritePass());
  pm.addPass(createSymbolDCEPass());
  pm.addNestedPass<func::FuncOp>(
      TFDevice::CreateReplicateInvariantOpHoistingPass());
  pm.addPass(CreateTPUMergeVariablesWithExecutePass());
  pm.addNestedPass<func::FuncOp>(
      TF::CreateHoistReplicateInvariantResourceWritesPass());
  pm.addNestedPass<func::FuncOp>(CreateTPUColocateCompositeResourceOps());
  pm.addPass(CreateTPUVariableRuntimeReformattingPass());
  pm.addPass(TF::CreateTFRegionControlFlowToFunctional());
}
}  // namespace

void CreateTPUBridgePipeline(OpPassManager &pm) {
  pm.addNestedPass<func::FuncOp>(
      CreateCanonicalizeCompileAndReplicateAttributesPass());
  CreateTPUBridgePipelineImpl(pm);
}

void CreateTPUBridgePipelineV1(OpPassManager &pm) {
  // Convert to unified compilation and replication attributes.
  pm.addNestedPass<func::FuncOp>(
      CreateCanonicalizeCompileAndReplicateAttributesPass());
  // Guarantee all functions have one use, which enables more exact shape
  // inference.
  pm.addPass(mlir::TF::CreateGuaranteeAllFuncsOneUsePass());
  pm.addPass(TF::CreateTFShapeInferencePass());
  // For V1 compatibility, we process a module where the graph does not have
  // feeds and fetched. We extract first the TPU computation in a submodule,
  // where it'll be in a function with args and returned values, much more like
  // a TF v2 module. We can then run the usual pipeline on this nested module.
  // Afterward we inline back in the parent module and delete the nested one.
  pm.addPass(tf_executor::CreateTFExecutorTPUV1IslandCoarseningPass());
  pm.addPass(tf_executor::CreateTFExecutorTPUV1IslandOutliningPass());
  OpPassManager &nested_module = pm.nest<ModuleOp>();
  CreateTPUBridgePipelineImpl(nested_module);
  pm.addPass(tf_executor::CreateTFExecutorTPUV1IslandInliningPass());
  // There are cases where we don't consume all compilation and replication
  // attributes like we do for the V2 pipeline, so we need to convert them from
  // unified to legacy attributes before they get exposed to outside of the
  // bridge.
  pm.addNestedPass<func::FuncOp>(
      CreateConvertToLegacyCompileAndReplicateAttributesPass());
}

tensorflow::Status TPUBridge(ModuleOp module, bool enable_logging,
                             bool fallback_enabled) {
  Status status =
      RunTFXLABridge(module, enable_logging, CreateTPUBridgePipeline);
  tensorflow::metrics::UpdateTfMlirBridgeFirstPhaseCounter(
      "tpu", "v2", fallback_enabled,
      status == Status::OK() ? "success" : "failure");
  OkOrSetErrorCounterPayload(
      tensorflow::core::platform::ErrorSourceProto::MLIR_BRIDGE_PHASE_1,
      status);
  return status;
}
tensorflow::Status TPUBridgeV1Compat(ModuleOp module, bool enable_logging,
                                     bool fallback_enabled) {
  Status status =
      RunTFXLABridge(module, enable_logging, CreateTPUBridgePipelineV1);
  tensorflow::metrics::UpdateTfMlirBridgeFirstPhaseCounter(
      "tpu", "v1", fallback_enabled,
      status == Status::OK() ? "success" : "failure");
  return status;
}

}  // namespace TFTPU

namespace TF {

void AddGraphExportLoweringPasses(OpPassManager &pm) {
  auto add_pass = [&](std::unique_ptr<Pass> pass) {
    pm.addNestedPass<func::FuncOp>(std::move(pass));
    pm.addPass(CreateBreakUpIslandsPass());
  };

  add_pass(CreateFunctionalToExecutorDialectConversionPass());
  add_pass(TFDevice::CreateReplicateToIslandPass());
  add_pass(TFDevice::CreateReplicaIDToDeviceOrdinalPass());
  add_pass(TFDevice::CreateParallelExecuteToIslandsPass());
  add_pass(TFDevice::CreateLaunchToDeviceAttributePass());
  pm.addNestedPass<func::FuncOp>(TFTPU::CreateTPUDevicePropagationPass());
  pm.addPass(createSymbolDCEPass());
  if (tensorflow::GetMlirCommonFlags()
          ->tf_mlir_enable_convert_control_to_data_outputs_pass) {
    pm.addPass(tf_executor::CreateTFExecutorConvertControlToDataOutputsPass());
  }
  pm.addPass(CreateVerifySuitableForExportPass());
}

tensorflow::Status RunBridgeWithStandardPipeline(ModuleOp module,
                                                 bool enable_logging,
                                                 bool enable_inliner) {
  PassManager bridge(module.getContext());
  if (enable_logging || VLOG_IS_ON(1)) {
    tensorflow::DumpMlirOpToFile("standard_pipeline_before", module);
    if (VLOG_IS_ON(2)) EnableDetailedLogging(&bridge);
  }

  StandardPipelineOptions pipeline_options;
  pipeline_options.enable_inliner.setValue(enable_inliner);
  CreateTFStandardPipeline(bridge, pipeline_options);

  mlir::StatusScopedDiagnosticHandler diag_handler(
      module.getContext(), /*propagate=*/false,
      /*filter_stack=*/!VLOG_IS_ON(1));

  LogicalResult result = bridge.run(module);
  (void)result;
  if (enable_logging || VLOG_IS_ON(1))
    tensorflow::DumpMlirOpToFile("standard_pipeline_after", module);
  return diag_handler.ConsumeStatus();
}

namespace {
void CreateTFXLABridgePipeline(OpPassManager &pm) {
  // The following ops must be preserved regardless of reachability. Ideally,
  // all graphs should have control dependencies to enforce this.
  VLOG(2) << "Create TF XLA Bridge pipeline";
  const llvm::SmallVector<std::string, 4> ops_to_preserve = {};
  pm.addNestedPass<func::FuncOp>(
      tf_executor::CreateTFExecutorGraphPruningPass(ops_to_preserve));
  // It is assumed at this stage there are no V1 control flow ops as Graph
  // functionalization is ran before import. Ops can be lifted out of
  // tf_executor dialect islands/graphs.
  pm.addNestedPass<func::FuncOp>(
      CreateExecutorDialectToFunctionalConversionPass());
  // Guarantee all functions have one use, which enables more exact shape
  // inference.
  pm.addPass(TF::CreateTFShapeInferencePass());
  // Running canonicalizer before decomposing resource ops in cluster helps the
  // latter pass to converge faster as it does not have to spend time folding
  // away dead ops.
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());
  // Encapsulate StatefulPartitionedCallOp within a cluster so that the
  // composite resource ops can be decomposed.
  pm.addPass(TFDevice::CreateXlaClusterFormationPass());
  // Place DecomposeResourceOpsPass.
  pm.addPass(TFDevice::CreateDecomposeResourceOpsInClusterPass());
  // Run another shape inference pass because resource decomposition might have
  // created new partial types. Also, after dropping `shape_invariant` attribute
  // from While/WhileRegion ops within cluster would lead to more precise
  // shapes.
  pm.addPass(TF::CreateTFShapeInferencePass());
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());
  pm.addPass(TFDevice::CreateResourceOpLiftingPass());
  // Inline the StatefulPartitionedCallOp op based in the parent region.
  pm.addPass(TFDevice::CreateXlaInlineDeviceOpsPass());
  // Re-run the canonicalizer pass as some cleanup during resource op lifting
  // pass opens up some opportunities for canonicalization of cluster ops.
  // Specifically, we want to eliminate pass through results from the cluster
  // op.
  pm.addNestedPass<func::FuncOp>(createCanonicalizerPass());

  pm.addNestedPass<func::FuncOp>(createCSEPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(TF::CreateTFRegionControlFlowToFunctional());
}

}  // namespace

tensorflow::Status RunTFXLABridge(ModuleOp module, bool enable_logging) {
  Status status = mlir::TFTPU::RunTFXLABridge(module, enable_logging,
                                              CreateTFXLABridgePipeline);
  tensorflow::metrics::UpdateTfMlirBridgeFirstPhaseCounter(
      /*device type*/ "cpu/gpu", /*bridge version*/ "tfxla",
      /*fallback_enabled*/ false,
      /*result*/ status == Status::OK() ? "success" : "failure");
  return status;
}

}  // namespace TF
}  // namespace mlir
