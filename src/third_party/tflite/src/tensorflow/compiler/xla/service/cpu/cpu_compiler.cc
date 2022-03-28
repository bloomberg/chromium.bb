/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/service/cpu/cpu_compiler.h"

#include <stddef.h>
#include <string.h>

#include <functional>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// IWYU pragma: no_include "llvm/Config/Disassemblers.def.inc"
// IWYU pragma: no_include "llvm/Config/Targets.def.inc"
#include "absl/base/call_once.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "mlir/Conversion/AffineToStandard/AffineToStandard.h"  // from @llvm-project
#include "mlir/Conversion/ArithmeticToLLVM/ArithmeticToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/BufferizationToMemRef/BufferizationToMemRef.h"  // from @llvm-project
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"  // from @llvm-project
#include "mlir/Conversion/MathToLLVM/MathToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"  // from @llvm-project
#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"  // from @llvm-project
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"  // from @llvm-project
#include "mlir/Conversion/ShapeToStandard/ShapeToStandard.h"  // from @llvm-project
#include "mlir/Conversion/VectorToSCF/VectorToSCF.h"  // from @llvm-project
#include "mlir/Dialect/Affine/IR/AffineOps.h"  // from @llvm-project
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"  // from @llvm-project
#include "mlir/Dialect/Arithmetic/Transforms/Passes.h"  // from @llvm-project
#include "mlir/Dialect/Bufferization/Transforms/Passes.h"  // from @llvm-project
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/Dialect/Func/Transforms/Passes.h"  // from @llvm-project
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"  // from @llvm-project
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"  // from @llvm-project
#include "mlir/Dialect/Linalg/IR/Linalg.h"  // from @llvm-project
#include "mlir/Dialect/Linalg/Passes.h"  // from @llvm-project
#include "mlir/Dialect/MemRef/Transforms/Passes.h"  // from @llvm-project
#include "mlir/Dialect/SCF/SCF.h"  // from @llvm-project
#include "mlir/Dialect/Shape/Transforms/Passes.h"  // from @llvm-project
#include "mlir/Dialect/Tensor/IR/Tensor.h"  // from @llvm-project
#include "mlir/Dialect/Vector/IR/VectorOps.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/ImplicitLocOpBuilder.h"  // from @llvm-project
#include "mlir/InitAllDialects.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"  // from @llvm-project
#include "mlir/Target/LLVMIR/Export.h"  // from @llvm-project
#include "mlir/Target/LLVMIR/LLVMTranslationInterface.h"  // from @llvm-project
#include "mlir/Transforms/Passes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/transforms/passes.h"
#include "tensorflow/compiler/mlir/tools/kernel_gen/transforms/passes.h"
#include "tensorflow/compiler/mlir/xla/hlo_to_mlir_hlo.h"
#include "tensorflow/compiler/mlir/xla/ir/xla_framework.h"
#include "tensorflow/compiler/mlir/xla/transforms/xla_passes.h"
#include "tensorflow/compiler/xla/cpu_function_runtime.h"
#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/map_util.h"
#include "tensorflow/compiler/xla/protobuf_util.h"
#include "tensorflow/compiler/xla/service/algebraic_simplifier.h"
#include "tensorflow/compiler/xla/service/all_gather_decomposer.h"
#include "tensorflow/compiler/xla/service/all_to_all_decomposer.h"
#include "tensorflow/compiler/xla/service/batch_dot_simplification.h"
#include "tensorflow/compiler/xla/service/batchnorm_expander.h"
#include "tensorflow/compiler/xla/service/bfloat16_normalization.h"
#include "tensorflow/compiler/xla/service/bitcast_dtypes_expander.h"
#include "tensorflow/compiler/xla/service/buffer_assignment.h"
#include "tensorflow/compiler/xla/service/call_inliner.h"
#include "tensorflow/compiler/xla/service/cholesky_expander.h"
#include "tensorflow/compiler/xla/service/comparison_expander.h"
#include "tensorflow/compiler/xla/service/conditional_canonicalizer.h"
#include "tensorflow/compiler/xla/service/conditional_simplifier.h"
#include "tensorflow/compiler/xla/service/conditional_to_select.h"
#include "tensorflow/compiler/xla/service/convolution_group_converter.h"
#include "tensorflow/compiler/xla/service/copy_insertion.h"
#include "tensorflow/compiler/xla/service/cpu/buffer_info_util.h"
#include "tensorflow/compiler/xla/service/cpu/compiler_functor.h"
#include "tensorflow/compiler/xla/service/cpu/conv_canonicalization.h"
#include "tensorflow/compiler/xla/service/cpu/cpu_executable.h"
#include "tensorflow/compiler/xla/service/cpu/cpu_instruction_fusion.h"
#include "tensorflow/compiler/xla/service/cpu/cpu_layout_assignment.h"
#include "tensorflow/compiler/xla/service/cpu/cpu_options.h"
#include "tensorflow/compiler/xla/service/cpu/dot_op_emitter.h"
#include "tensorflow/compiler/xla/service/cpu/ir_emission_utils.h"
#include "tensorflow/compiler/xla/service/cpu/ir_emitter.h"
#include "tensorflow/compiler/xla/service/cpu/parallel_task_assignment.h"
#include "tensorflow/compiler/xla/service/cpu/simple_orc_jit.h"
#include "tensorflow/compiler/xla/service/dfs_hlo_visitor_with_default.h"
#include "tensorflow/compiler/xla/service/dot_decomposer.h"
#include "tensorflow/compiler/xla/service/dump.h"
#include "tensorflow/compiler/xla/service/dynamic_dimension_simplifier.h"
#include "tensorflow/compiler/xla/service/dynamic_index_splitter.h"
#include "tensorflow/compiler/xla/service/dynamic_padder.h"
#include "tensorflow/compiler/xla/service/eigh_expander.h"
#include "tensorflow/compiler/xla/service/flatten_call_graph.h"
#include "tensorflow/compiler/xla/service/gather_expander.h"
#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/service/hlo_casting_utils.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_constant_folding.h"
#include "tensorflow/compiler/xla/service/hlo_cse.h"
#include "tensorflow/compiler/xla/service/hlo_dce.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_instructions.h"
#include "tensorflow/compiler/xla/service/hlo_memory_scheduler.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/hlo_ordering.h"
#include "tensorflow/compiler/xla/service/hlo_pass_fix.h"
#include "tensorflow/compiler/xla/service/hlo_pass_pipeline.h"
#include "tensorflow/compiler/xla/service/hlo_proto_util.h"
#include "tensorflow/compiler/xla/service/hlo_subcomputation_unification.h"
#include "tensorflow/compiler/xla/service/hlo_verifier.h"
#include "tensorflow/compiler/xla/service/indexed_array_analysis.h"
#include "tensorflow/compiler/xla/service/llvm_compiler.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_command_line_options.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_util.h"
#include "tensorflow/compiler/xla/service/logistic_expander.h"
#include "tensorflow/compiler/xla/service/map_inliner.h"
#include "tensorflow/compiler/xla/service/operand_upcaster.h"
#include "tensorflow/compiler/xla/service/optimization_barrier_expander.h"
#include "tensorflow/compiler/xla/service/qr_expander.h"
#include "tensorflow/compiler/xla/service/reduce_scatter_decomposer.h"
#include "tensorflow/compiler/xla/service/reshape_mover.h"
#include "tensorflow/compiler/xla/service/result_caster.h"
#include "tensorflow/compiler/xla/service/rng_bit_generator_expander.h"
#include "tensorflow/compiler/xla/service/rng_expander.h"
#include "tensorflow/compiler/xla/service/scatter_expander.h"
#include "tensorflow/compiler/xla/service/sharding_propagation.h"
#include "tensorflow/compiler/xla/service/sharding_remover.h"
#include "tensorflow/compiler/xla/service/slice_sinker.h"
#include "tensorflow/compiler/xla/service/slow_operation_alarm.h"
#include "tensorflow/compiler/xla/service/sort_simplifier.h"
#include "tensorflow/compiler/xla/service/spmd/stateful_rng_spmd_partitioner.h"
#include "tensorflow/compiler/xla/service/topk_rewriter.h"
#include "tensorflow/compiler/xla/service/transpose_folding.h"
#include "tensorflow/compiler/xla/service/tree_reduction_rewriter.h"
#include "tensorflow/compiler/xla/service/triangular_solve_expander.h"
#include "tensorflow/compiler/xla/service/tuple_simplifier.h"
#include "tensorflow/compiler/xla/service/while_loop_constant_sinking.h"
#include "tensorflow/compiler/xla/service/while_loop_invariant_code_motion.h"
#include "tensorflow/compiler/xla/service/while_loop_simplifier.h"
#include "tensorflow/compiler/xla/service/zero_sized_hlo_elimination.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"

namespace {

// We need to explicitly load all the dialects we will involved in emitting the
// IR. This is only needed because of how MLIR is bolted into XLA and does not
// make use of the MLIR infrastructure (like using a proper pass pipeline).
// Hopefully this will all go away at some point in favor of a better
// integration.
void LoadMLIRDialects(mlir::MLIRContext& context) {
  context.loadDialect<mlir::arith::ArithmeticDialect,
                      mlir::linalg::LinalgDialect, mlir::scf::SCFDialect,
                      mlir::vector::VectorDialect, mlir::func::FuncDialect,
                      mlir::AffineDialect, mlir::tensor::TensorDialect,
                      mlir::xla_framework::XLAFrameworkDialect>();
  mlir::registerLLVMDialectTranslation(context);
}

}  // namespace

namespace xla {

namespace {

bool UseMlirHloLowering(bool use_mlir, HloModule* module) {
  // TODO(tpopp): The prototype currently does not properly handle constant
  // buffers that are handled by the runtime's buffer assignmen.
  return use_mlir &&
         module->entry_computation()->root_instruction()->opcode() !=
             HloOpcode::kConstant;
}

// For each computation in the module, determines whether that computation
// calls a custom-call function, either directly or indirectly (e.g. because it
// calls another computation that does).
absl::flat_hash_map<const HloComputation*, bool>
ModuleComputationsTransitivelyContainCustomCall(const HloModule& module) {
  absl::flat_hash_map<const HloComputation*, bool> custom_call_map;
  std::unique_ptr<CallGraph> call_graph = CallGraph::Build(&module);

  // Can never fail because we always return an OK status from the visitor.
  TF_CHECK_OK(call_graph->VisitNodes([&custom_call_map](
                                         const CallGraphNode& node) {
    const HloComputation* computation = node.computation();

    for (const HloInstruction* instruction : computation->instructions()) {
      // The computation contains a custom-call instruction directly.
      if (DynCast<HloCustomCallInstruction>(instruction)) {
        custom_call_map[computation] = true;
        return Status::OK();
      }
      // The computation calls something that contains a custom-call
      // instruction (directly or indirectly). This lookup relies on the call
      // graph traversing callees before callers, so that the map is always
      // populated for all callees at this point.
      for (const HloComputation* callee : instruction->called_computations()) {
        bool callee_contains_custom_call = FindOrDie(custom_call_map, callee);
        if (callee_contains_custom_call) {
          custom_call_map[computation] = true;
          return Status::OK();
        }
      }
    }

    custom_call_map[computation] = false;
    return Status::OK();
  }));

  return custom_call_map;
}

}  // namespace

namespace cpu {
using BufferInfo = cpu_function_runtime::BufferInfo;

CpuAotCompilationOptions::CpuAotCompilationOptions(
    std::string triple, std::string cpu_name, std::string features,
    std::string entry_point_name, RelocationModel relocation_model)
    : triple_(std::move(triple)),
      cpu_name_(std::move(cpu_name)),
      features_(std::move(features)),
      entry_point_name_(std::move(entry_point_name)),
      relocation_model_(relocation_model) {}

CpuAotCompilationOptions::~CpuAotCompilationOptions() = default;

se::Platform::Id CpuAotCompilationOptions::PlatformId() const {
  return se::host::kHostPlatformId;
}

CpuAotCompilationResult::CpuAotCompilationResult(
    ObjectFileData object_file_data, std::vector<BufferInfo> buffer_infos,
    int64_t result_buffer_index,
    std::unique_ptr<HloProfilePrinterData> hlo_profile_printer_data)
    : object_file_data_(std::move(object_file_data)),
      buffer_infos_(std::move(buffer_infos)),
      result_buffer_index_(result_buffer_index),
      hlo_profile_printer_data_(std::move(hlo_profile_printer_data)) {}

CpuAotCompilationResult::~CpuAotCompilationResult() = default;

CpuCompiler::CpuCompiler() {
  // Initialize LLVM the first time the CpuCompiler is initialized.
  static bool llvm_initialized = []() {
    InitializeLLVMTarget();
    return true;
  }();
  (void)llvm_initialized;
}

StatusOr<std::vector<std::unique_ptr<Executable>>> CpuCompiler::Compile(
    std::unique_ptr<HloModuleGroup> module_group,
    std::vector<std::vector<se::StreamExecutor*>> stream_execs,
    const CompileOptions& options) {
  for (const std::vector<se::StreamExecutor*>& se_vector : stream_execs) {
    if (se_vector.size() != 1) {
      return Unimplemented(
          "Model partitioning not implemented for the CPU compiler");
    }
  }
  return LLVMCompiler::Compile(std::move(module_group), stream_execs, options);
}

/* static */ void CpuCompiler::InitializeLLVMTarget() {
  // Initialize LLVM's MC layer for the native target.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
}

namespace {

// LLVM makes certain options configurable only through its command-line
// options; it provide the ParseCommandLineOptions function that lets us set
// flags at runtime. However, since these flags are global we want to avoid
// multiple invocations of the LLVM compilation pipeline with a different set of
// flags. Therefore, we only pass command-line flags to LLVM once, before the
// first module is compiled.
absl::once_flag llvm_command_line_options_initialized;

// This visitor records which HLO instructions should have profiling information
// recorded.
class CollectProfileCandidates : public DfsHloVisitorWithDefault {
 public:
  static StatusOr<absl::flat_hash_map<const HloInstruction*, int64_t>>
  GetCandidatesForComputation(
      const HloComputation& computation,
      const absl::flat_hash_map<const HloInstruction*, int64_t>&
          assigned_indices) {
    absl::flat_hash_map<const HloInstruction*, int64_t> hlo_to_profile_idx;
    CollectProfileCandidates profile_candidates_for_computation(
        &hlo_to_profile_idx, assigned_indices);
    TF_RETURN_IF_ERROR(computation.Accept(&profile_candidates_for_computation));
    return hlo_to_profile_idx;
  }

 private:
  CollectProfileCandidates(
      absl::flat_hash_map<const HloInstruction*, int64_t>* hlo_to_profile_idx,
      const absl::flat_hash_map<const HloInstruction*, int64_t>&
          assigned_indices)
      : hlo_to_profile_idx_(hlo_to_profile_idx),
        assigned_indices_(assigned_indices) {}

  Status DefaultAction(HloInstruction* hlo_instruction) override {
    hlo_to_profile_idx_->insert(
        {hlo_instruction, FindOrDie(assigned_indices_, hlo_instruction)});
    return Status::OK();
  }

  Status HandleCall(HloInstruction* call) override {
    TF_RETURN_IF_ERROR(DefaultAction(call));
    CollectProfileCandidates candidates_for_call(hlo_to_profile_idx_,
                                                 assigned_indices_);
    TF_RETURN_IF_ERROR(call->to_apply()->Accept(&candidates_for_call));
    return Status::OK();
  }
  // Recurse into "conditional" so we can profile inside of it.
  Status HandleConditional(HloInstruction* conditional) override {
    TF_RETURN_IF_ERROR(DefaultAction(conditional));

    CollectProfileCandidates candidates_for_true(hlo_to_profile_idx_,
                                                 assigned_indices_);
    TF_RETURN_IF_ERROR(
        conditional->true_computation()->Accept(&candidates_for_true));

    CollectProfileCandidates candidates_for_false(hlo_to_profile_idx_,
                                                  assigned_indices_);
    TF_RETURN_IF_ERROR(
        conditional->false_computation()->Accept(&candidates_for_false));

    return Status::OK();
  }

  // Skip constants, there is nothing to profile.
  Status HandleConstant(HloInstruction*) override { return Status::OK(); }
  // Skip parameters, they are a simple load.
  Status HandleParameter(HloInstruction*) override { return Status::OK(); }
  // It is important to recurse for "while" or else we risk overly coarse
  // profiling information.
  Status HandleWhile(HloInstruction* xla_while) override {
    TF_RETURN_IF_ERROR(DefaultAction(xla_while));

    CollectProfileCandidates candidates_for_condition(hlo_to_profile_idx_,
                                                      assigned_indices_);
    TF_RETURN_IF_ERROR(
        xla_while->while_condition()->Accept(&candidates_for_condition));

    CollectProfileCandidates candidates_for_body(hlo_to_profile_idx_,
                                                 assigned_indices_);
    TF_RETURN_IF_ERROR(xla_while->while_body()->Accept(&candidates_for_body));

    return Status::OK();
  }

  absl::flat_hash_map<const HloInstruction*, int64_t>* hlo_to_profile_idx_;
  const absl::flat_hash_map<const HloInstruction*, int64_t>& assigned_indices_;
};

}  // namespace

Status CpuCompiler::RunHloPassesThroughLayoutAssn(
    HloModule* module, bool /*is_aot_compile*/,
    LLVMTargetMachineFeatures* target_machine_features) {
  if (module->config().use_spmd_partitioning()) {
    HloPassPipeline spmd_pipeline("spmd-partitioner");
    const int64_t num_partitions = module->config().num_partitions();
    if (num_partitions > 1) {
      // Run some IR cleanup passes before running the SPMD partitioning
      // passes.
      spmd_pipeline.AddInvariantChecker<HloVerifier>(
          /*layout_sensitive=*/false,
          /*allow_mixed_precision=*/false);
      spmd_pipeline.AddPass<CallInliner>();
      spmd_pipeline.AddPass<ZeroSizedHloElimination>();
      spmd_pipeline.AddPass<ConditionalCanonicalizer>();

      spmd_pipeline.AddPass<ShardingPropagation>(/*is_spmd=*/true);
      spmd_pipeline.AddPass<spmd::StatefulRngSpmdPartitioner>(
          num_partitions, module->config().replica_count());
    } else {
      // Remove redundant sharding ops when partition_count == 1.
      spmd_pipeline.AddPass<ShardingRemover>();
      spmd_pipeline.AddPass<HloDCE>();
    }
    TF_RETURN_IF_ERROR(spmd_pipeline.Run(module).status());
  }

  HloPassPipeline pipeline("HLO passes through layout assignment");
  pipeline.AddInvariantChecker<HloVerifier>(/*layout_sensitive=*/false,
                                            /*allow_mixed_precision=*/false);

  pipeline.AddPass<OperandUpcaster>();
  pipeline.AddPass<ResultCaster>();

  // Expand random number generation.
  pipeline.AddPass<RngExpander>();
  pipeline.AddPass<RngBitGeneratorExpander>(RandomAlgorithm::RNG_PHILOX);

  // Remove zero-sized HLO from the input so that other passes don't have to
  // handle it.
  pipeline.AddPass<ZeroSizedHloElimination>();

  pipeline.AddPass<DynamicIndexSplitter>();

  pipeline.AddPass<ConditionalToSelect>();
  pipeline.AddPass<MapInliner>();

  pipeline.AddPass<ComparisonExpander>();
  pipeline.AddPass<CholeskyExpander>();
  pipeline.AddPass<QrExpander>();
  pipeline.AddPass<EighExpander>();
  pipeline.AddPass<TriangularSolveExpander>();
  pipeline.AddPass<AllGatherDecomposer>();
  pipeline.AddPass<AllToAllDecomposer>();
  pipeline.AddPass<ReduceScatterDecomposer>();

  // Inline computations with a single call site.
  pipeline.AddPass<CallInliner>(/*single_call_site=*/true);
  pipeline.AddPass<BatchDotSimplification>();
  pipeline.AddPass<DotDecomposer>();
  // Convert BF16 operations to F32 operations so that the CPU backend can
  // support BF16 operations without directly implementing a BF16 lowering for
  // most ops.
  BFloat16Support bf16;
  pipeline.AddPass<BFloat16Normalization>(&bf16);
  // After canonicalization, there may be more batch dots that can be
  // simplified.
  pipeline.AddPass<BatchDotSimplification>();
  auto cost_model = [](HloInstruction* conv) {
    // We need a cost model for CPUs. Currently, do nothing.
    return false;
  };
  pipeline.AddPass<ConvolutionGroupConverter>(
      /*should_expand=*/[](HloInstruction* conv) { return true; }, cost_model,
      /*convert_batch_groups_only=*/true);
  auto feature_group_should_expand = [](HloInstruction* conv) {
    switch (conv->shape().element_type()) {
      case F16:
      case F32:
        return false;
      default:
        return true;
    }
  };
  pipeline.AddPass<ConvolutionGroupConverter>(
      feature_group_should_expand, cost_model,
      /*convert_batch_groups_only=*/false);
  pipeline.AddPass<BatchNormExpander>(
      /*rewrite_training_op=*/true,
      /*rewrite_inference_op=*/true,
      /*rewrite_grad_op=*/true);
  pipeline.AddPass<LogisticExpander>(
      /*expansion_type=*/LogisticExpansionType::kExp);
  pipeline.AddPass<ConditionalCanonicalizer>();
  pipeline.AddPass<DynamicDimensionSimplifier>();
  auto dynamic_padder_options = DynamicPadderOptions();
  dynamic_padder_options.shape_check_mode =
      DynamicDimensionInference::ShapeCheckMode::kCompileTime;
  pipeline.AddPass<DynamicPadder>(dynamic_padder_options);
  pipeline.AddPass<ScatterExpander>(ScatterExpander::kEliminateAllScatters);
  pipeline.AddPass<ConvCanonicalization>(target_machine_features);

  // Run the following passes to a fixed point.
  [&pipeline =
       pipeline.AddPass<HloPassFix<HloPassPipeline>>("simplification")] {
    pipeline.AddInvariantCheckerDebug<HloVerifier>(
        /*layout_sensitive=*/false,
        /*allow_mixed_precision=*/false);

    AlgebraicSimplifierOptions options;
    options.set_enable_dot_strength_reduction(false);
    // TODO(b/209827141): XLA:CPU doesn't propagate NaN through min/max, but
    // other platforms do, so it should be changed.
    options.set_minmax_propagate_nan(false);
    pipeline.AddPass<AlgebraicSimplifier>(options);
    pipeline.AddPass<SortSimplifier>();
    pipeline.AddPass<HloDCE>();
    pipeline.AddPass<GatherExpander>(GatherExpander::kEliminateSimpleGathers);

    // Needs to happen after algebraic simplifier.
    pipeline.AddPass<TreeReductionRewriter>();

    // BatchNormExpander can create zero-sized ops, so zero-sized HLO
    // elimination has to come after that pass.
    pipeline.AddPass<ZeroSizedHloElimination>();

    pipeline.AddPass<WhileLoopInvariantCodeMotion>();
    pipeline.AddPass<TupleSimplifier>();
    pipeline.AddPass<WhileLoopConstantSinking>();
    pipeline.AddPass<WhileLoopSimplifier>();

    // TODO(b/134075051): Re-enable after b/134075051 is fixed.
    // pipeline.AddPass<SliceSinker>();

    pipeline.AddPass<HloDCE>();
    pipeline.AddPass<ReshapeMover>();
    pipeline.AddPass<HloConstantFolding>();
    pipeline.AddPass<ConditionalSimplifier>();
  }();
  pipeline.AddPass<BitcastDtypesExpander>();

  pipeline.AddPass<TopkRewriter>([](const HloSortInstruction* sort, int64_t) {
    return sort->operand(0)->shape().element_type() == F32;
  });
  pipeline.AddPass<IndexedArrayAnalysisPrinterPass>();
  pipeline.AddPass<TransposeFolding>(
      [&](const HloInstruction& dot,
          const TransposeFolding::OperandIndices& candidate_operands) {
        return DotImplementationCanHandleTranspose(dot,
                                                   *target_machine_features)
                   ? candidate_operands
                   : TransposeFolding::OperandIndices{};
      },
      TransposeFolding::NeverFoldTranspose);
  pipeline.AddPass<HloCSE>(/*is_layout_sensitive=*/false);

  pipeline.AddPass<OptimizationBarrierExpander>();
  pipeline.AddPass<TupleSimplifier>();

  // Layout assignment uses alias analysis, which requires the call graph to be
  // flattened.
  pipeline.AddPass<FlattenCallGraph>();
  ChannelLayoutConstraints layout_constraints;
  pipeline.AddPass<CpuLayoutAssignment>(
      module->mutable_entry_computation_layout(), target_machine_features,
      &layout_constraints);

  return pipeline.Run(module).status();
}

Status CpuCompiler::RunHloPassesAfterLayoutAssn(
    HloModule* module, bool is_aot_compile,
    LLVMTargetMachineFeatures* target_machine_features, bool is_mlir_compile) {
  HloPassPipeline pipeline("HLO passes after layout assignment");

  // CopyInsertion is still needed by BufferAssignment. MLIR passes will handle
  // everything else done by XLA, but CopyInsertion is needed to interface with
  // the existing runtime.
  if (is_mlir_compile) {
    pipeline.AddPass<CopyInsertion>();
    return pipeline.Run(module).status();
  }

  // After layout assignment, use a layout-sensitive verifier.
  pipeline.AddPass<HloPassPipeline>("after layout assignment")
      .AddInvariantCheckerDebug<HloVerifier>(
          /*layout_sensitive=*/true,
          /*allow_mixed_precision=*/false);

  // Add a fusion pass now that layout assignment is done.
  pipeline.AddPass<CpuInstructionFusion>();

  // The LayoutAssignment pass may leave behind kCopy instructions which are
  // duplicate or NOPs, so remove them with algebraic simplification and CSE.
  // Run this to a fixed point.
  [&pipeline = pipeline.AddPass<HloPassFix<HloPassPipeline>>(
       "simplification after layout assignment")] {
    pipeline.AddInvariantCheckerDebug<HloVerifier>(
        /*layout_sensitive=*/true,
        /*allow_mixed_precision=*/false,
        LayoutAssignment::InstructionCanChangeLayout);
    AlgebraicSimplifierOptions options;
    options.set_is_layout_sensitive(true);
    options.set_enable_dot_strength_reduction(false);
    // TODO(b/209827141): XLA:CPU doesn't propagate NaN through min/max, but
    // other platforms do, so it should be changed.
    options.set_minmax_propagate_nan(false);
    pipeline.AddPass<AlgebraicSimplifier>(options);
    pipeline.AddPass<HloDCE>();
    pipeline.AddPass<HloCSE>(/*is_layout_sensitive=*/true);
  }();

  // Outline ops in the entry computation into calls to subcomputations.
  const int max_parallelism =
      module->config().intra_op_parallelism_threads() > 0
          ? module->config().intra_op_parallelism_threads()
          : tensorflow::port::NumSchedulableCPUs();
  if (!is_aot_compile) {
    // Run ParallelTaskAssigner to assign parallel tasks to HLOs in module.
    // Note this is not run for AOT because it would bring in thread pool
    // and thread synchronization dependencies which would likely increase
    // binary size (and most AOT applications are single-threaded).
    // TODO(b/29630486) Support multi-threaded AOT.
    pipeline.AddPass<ParallelTaskAssigner>(
        max_parallelism, ShapeSizeBytesFunction(), target_machine_features);
  }
  // Copy insertion should be performed immediately before IR emission to
  // avoid inserting unnecessary copies (later pass adds an instruction which
  // materializes the value) or missing a necessary copy (later pass removes
  // an instruction which materializes a value). DCE must be run immediately
  // before (and sometime after) copy insertion, to avoid dead code from
  // interfering with the rewrites.
  pipeline.AddPass<HloDCE>();
  pipeline.AddPass<CopyInsertion>();
  pipeline.AddPass<HloDCE>();
  return pipeline.Run(module).status();
}

Status CpuCompiler::RunHloPasses(HloModule* module, bool is_aot_compile,
                                 llvm::TargetMachine* target_machine,
                                 bool is_mlir_compile) {
  if (DumpingEnabledForHloModule(*module)) {
    hlo_proto_ = absl::make_unique<HloProto>();
    *hlo_proto_->mutable_hlo_module() = module->ToProto();
  }

  LLVMTargetMachineFeatures target_machine_features(target_machine);
  TF_RETURN_IF_ERROR(RunHloPassesThroughLayoutAssn(module, is_aot_compile,
                                                   &target_machine_features));

  return RunHloPassesAfterLayoutAssn(
      module, is_aot_compile, &target_machine_features,
      UseMlirHloLowering(is_mlir_compile, module));
}

namespace {

// Align buffers to 16-byte boundaries.
int64_t memory_alignment(LogicalBuffer::Color) {
  return cpu_function_runtime::MinAlign();
}

llvm::TargetOptions CompilerTargetOptions(
    const HloModuleConfig& module_config) {
  llvm::TargetOptions target_options;
  // Always allow FMA fusion. This increases precision instead of decreasing it.
  target_options.AllowFPOpFusion = llvm::FPOpFusion::Fast;
  return target_options;
}

llvm::CodeGenOpt::Level CodeGenOptLevel(const HloModuleConfig& module_config) {
  VLOG(2) << "backend_optimization_level: "
          << module_config.debug_options().xla_backend_optimization_level();
  switch (module_config.debug_options().xla_backend_optimization_level()) {
    case 1:
      return llvm::CodeGenOpt::Less;
    case 2:
      return llvm::CodeGenOpt::Default;
    case 3:
      return llvm::CodeGenOpt::Aggressive;
    default:
      return llvm::CodeGenOpt::None;
  }
}

std::pair<LLVMCompiler::ModuleHook, LLVMCompiler::ModuleHook> GetIRModuleHooks(
    const HloModule& hlo_module,
    const LLVMCompiler::ModuleHook& user_pre_optimization_hook,
    const LLVMCompiler::ModuleHook& user_post_optimization_hook) {
  // Create the IR hooks. If applicable, each IR hook does the following:
  //
  //  * Calls the user supplied module hook.
  //  * Writes out the IR to a file in the output directory designated by
  //    --xla_dump_to
  const HloModule* hlo_module_ptr = &hlo_module;
  auto hook = [user_pre_optimization_hook, user_post_optimization_hook,
               hlo_module_ptr](bool optimized,
                               const llvm::Module& llvm_module) {
    const auto& user_hook =
        !optimized ? user_pre_optimization_hook : user_post_optimization_hook;
    if (user_hook) {
      user_hook(llvm_module);
    }
    llvm_ir::DumpIrIfEnabled(*hlo_module_ptr, llvm_module, optimized);
  };
  return {[hook](const llvm::Module& llvm_module) {
            return hook(/*optimized=*/false, llvm_module);
          },
          [hook](const llvm::Module& llvm_module) {
            return hook(/*optimized=*/true, llvm_module);
          }};
}

Status VerifyLlvmModule(const llvm::Module& llvm_module) {
  XLA_SCOPED_LOGGING_TIMER("CpuCompiler - Running LLVM verifier");

  std::string err;
  llvm::raw_string_ostream err_stream(err);

  // verifyModule() returns true if the module is broken.
  TF_RET_CHECK(!llvm::verifyModule(llvm_module, &err_stream))
      << "Invalid LLVM IR before optimizations:\n"
      << err_stream.str()
      << "\nThis probably indicates a bug in the HLO -> LLVM IR lowering. "
         "Rerun with --xla_dump_to to get the IR. ";
  return Status::OK();
}

Status CreateHloProfilingArtifacts(
    const HloModule& module,
    absl::flat_hash_map<const HloInstruction*, int64_t>*
        instruction_to_profile_idx,
    absl::flat_hash_map<const HloComputation*, int64_t>*
        computation_to_profile_idx,
    std::unique_ptr<HloProfileIndexMap>* hlo_profile_index_map,
    std::unique_ptr<HloProfilePrinterData>* hlo_profile_printer_data) {
  *hlo_profile_index_map = absl::make_unique<HloProfileIndexMap>(module);
  const HloComputation& entry_computation = *module.entry_computation();

  TF_ASSIGN_OR_RETURN(
      *instruction_to_profile_idx,
      CollectProfileCandidates::GetCandidatesForComputation(
          entry_computation,
          (*hlo_profile_index_map)->instruction_to_profile_idx()));

  auto shape_size_bytes = [](const Shape& shape) {
    // On the cpu, opaques are pointers.
    if (shape.IsOpaque()) {
      return static_cast<int64_t>(sizeof(void*));
    }
    return ShapeUtil::ByteSizeOf(shape, sizeof(void*));
  };

  HloCostAnalysis cost_analysis(shape_size_bytes);
  TF_RETURN_IF_ERROR(entry_computation.Accept(&cost_analysis));
  *hlo_profile_printer_data = CreateHloProfilePrinterData(
      **hlo_profile_index_map, cost_analysis, entry_computation.name());
  *computation_to_profile_idx =
      (*hlo_profile_index_map)->computation_to_profile_idx();

  return Status::OK();
}

}  // namespace

StatusOr<std::unique_ptr<HloModule>> CpuCompiler::RunHloPasses(
    std::unique_ptr<HloModule> module, se::StreamExecutor* /*stream_exec*/,
    const CompileOptions& /*options*/) {
  std::unique_ptr<llvm::TargetMachine> jit_target_machine =
      SimpleOrcJIT::InferTargetMachineForJIT(
          CompilerTargetOptions(module->config()),
          CodeGenOptLevel(module->config()));

  TF_RETURN_IF_ERROR(RunHloPasses(module.get(), /*is_aot_compile=*/false,
                                  jit_target_machine.get()));
  return std::move(module);
}

StatusOr<std::unique_ptr<BufferAssignment>> CpuCompiler::AssignBuffers(
    const HloModule* module) {
  // Select an order for emitting the HLO instructions for each computation.
  // Using this sequence enables tighter buffer liveness analysis and reduced
  // memory usage (as compared to using DependencyHloOrdering).
  TF_ASSIGN_OR_RETURN(HloSchedule schedule,
                      ScheduleModule(module, BufferSizeBytesFunction(),
                                     ComputationSchedulerToModuleScheduler(
                                         DFSMemoryScheduler)));

  // Run buffer allocation on the HLO graph.
  TF_ASSIGN_OR_RETURN(
      std::unique_ptr<BufferAssignment> assignment,
      BufferAssigner::Run(module,
                          absl::make_unique<SequentialHloOrdering>(schedule),
                          BufferSizeBytesFunction(), memory_alignment,
                          /*allocate_buffers_for_constants=*/true));

  return std::move(assignment);
}

namespace {

// Post-compilation callback functor for use by SimpleOrcJIT.
//
// Dumps machine code if dumping is enabled for the module.
struct OrcJITPostCompilationHook {
  // Gets an std::function that implements this hook.
  static std::function<void(const llvm::object::ObjectFile& obj_file)> Create(
      const HloModule* module) {
    // This struct is not copyable, but std::functions must be.  So to create an
    // std::function out of this struct, we have to wrap it in a shared_ptr.
    auto wrapped = std::make_shared<OrcJITPostCompilationHook>(module);
    return [wrapped](const llvm::object::ObjectFile& obj_file) {
      (*wrapped)(obj_file);
    };
  }

  // Constructor can't be private because we want to call it from
  // std::make_shared, but users should call Create() instead.
  explicit OrcJITPostCompilationHook(const HloModule* module)
      : module(module) {}

 private:
  void operator()(const llvm::object::ObjectFile& obj_file) {
    if (!DumpingEnabledForHloModule(*module)) {
      return;
    }
    DumpToFileInDir(*module, /*file_prefix=*/"", /*file_suffix=*/"o",
                    absl::string_view(obj_file.getData().data(),
                                      obj_file.getData().size()));
  }

  const HloModule* module;
};

void InitializeLLVMCommandLineOptions(const HloModuleConfig& config) {
  llvm_ir::InitializeLLVMCommandLineOptions(
      config.debug_options().xla_backend_extra_options());
}

Status LowerMLIRModule(mlir::ModuleOp mlir_module,
                       mlir::MLIRContext& mlir_context) {
  LoadMLIRDialects(mlir_context);
  mlir::PassManager pm(&mlir_context);
  // Resolve all shape constraints (e.g. broadcast constraints that can be
  // proved statically and changed to const witness) early to allow more
  // efficient broadcast operations moving.
  // Move up broadcasting operations to allow for more fusion opportunities.
  pm.addPass(mlir::mhlo::CreateExpandHloTuplesPass("main"));
  pm.addNestedPass<mlir::FuncOp>(mlir::mhlo::createLegalizeGeneralDotPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::mhlo::createBroadcastPropagationPass());
  pm.addPass(mlir::createCSEPass());
  pm.addPass(mlir::createCanonicalizerPass());

  // Transform HLO operations to Linalg.
  pm.addPass(mlir::mhlo::createLegalizeToMemrefPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::mhlo::createLegalizeControlFlowPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::mhlo::createLegalizeHloToLinalgPass());

  // Lower index cast on tensors to tensor.generate.
  pm.addNestedPass<mlir::FuncOp>(
      mlir::kernel_gen::transforms::CreateLowerIndexCastPass());

  // Lower shape dialect to standard to enable linalg canonicalizations (e.g.
  // use linalg inputs instead of outputs for memref.dim operations).
  pm.addNestedPass<mlir::FuncOp>(
      mlir::kernel_gen::transforms::CreateShapeSimplification());
  pm.addNestedPass<mlir::FuncOp>(mlir::createShapeToShapeLowering());
  pm.addPass(mlir::createConvertShapeToStandardPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createConvertShapeConstraintsPass());

  // Fuse Linalg on tensors operations.
  pm.addPass(mlir::createCSEPass());
  pm.addPass(mlir::memref::createResolveShapedTypeResultDimsPass());
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createLinalgElementwiseOpFusionPass());
  pm.addPass(mlir::createReconcileUnrealizedCastsPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createLinalgBufferizePass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createConvertLinalgToLoopsPass());
  pm.addPass(mlir::createInlinerPass());

  // Bufferize Linalg on tensors program.
  // Always run canonicalizer (which does dead code removal) before
  // bufferizing anything.
  pm.addPass(mlir::createCanonicalizerPass());
  // Now bufferize all the compute operations (hlo + linalg) and func
  // signature.
  pm.addPass(
      mlir::kernel_gen::transforms::CreateComputeOpAndFuncBufferizePass());
  pm.addNestedPass<mlir::FuncOp>(
      mlir::kernel_gen::transforms::CreateTiledLoopBufferizePass());
  // Turn tensor constants into global memrefs.
  // TODO(kramerb): Expose the patterns and add them to the bufferize passes.
  // pm.addPass(mlir::createTensorConstantBufferizePass());
  // Always run canonicalizer (which does dead code removal) before
  // bufferizing anything.
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addPass(mlir::kernel_gen::transforms::CreateFinalBufferizePass(
      /*alignment=*/xla::cpu_function_runtime::Align()));
  pm.addPass(mlir::createCSEPass());
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addPass(mlir::bufferization::createBufferResultsToOutParamsPass());
  pm.addPass(mlir::mhlo::CreateOutlineWithXLAFrameworkPass());

  // Deallocate all temporary buffers.
  pm.addNestedPass<mlir::FuncOp>(
      mlir::bufferization::createBufferDeallocationPass());

  pm.addPass(mlir::createBufferizationToMemRefPass());

  // Specilize linalg.matmul to linalg.dot, linalg.matvec or linalg.vecmat,
  // and immediately canonicalize to clean up not taken branches.
  // pm.addNestedPass<mlir::FuncOp>(CreateLinalgMatmulSpecializationPass());
  pm.addPass(mlir::createCanonicalizerPass());

  // Tile and vectorize linalg operation using Linalg Codegen Strategy.
  // pm.addNestedPass<mlir::FuncOp>(CreateCodegenStrategyForMatMulPass());

  pm.addPass(mlir::createCSEPass());
  pm.addPass(mlir::createCanonicalizerPass());

  mlir::VectorTransferToSCFOptions vec_to_scf_options;
  vec_to_scf_options.unroll = true;
  pm.addNestedPass<mlir::FuncOp>(
      mlir::createConvertVectorToSCFPass(vec_to_scf_options));
  pm.addNestedPass<mlir::FuncOp>(mlir::arith::createArithmeticExpandOpsPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::memref::createExpandOpsPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createLowerAffinePass());
  pm.addPass(mlir::mhlo::CreateLegalizeXLAFrameworkToLLVMPass());
  pm.addPass(mlir::createMemRefToLLVMPass());
  pm.addPass(mlir::createConvertSCFToCFPass());
  pm.addNestedPass<mlir::FuncOp>(mlir::createConvertMathToLLVMPass());
  pm.addNestedPass<mlir::FuncOp>(
      mlir::arith::createConvertArithmeticToLLVMPass());
  pm.addPass(mlir::createConvertFuncToLLVMPass());
  pm.addPass(mlir::createReconcileUnrealizedCastsPass());
  if (pm.run(mlir_module).failed()) {
    mlir_module->dump();
    return tensorflow::errors::Internal(
        "Failed to compile through MLIR pipeline");
  }

  // Make @main private so it doesn't clash with other modules.
  mlir_module->walk([&](mlir::LLVM::LLVMFuncOp f) {
    if (f.getName() == "main") {
      f.setLinkageAttr(mlir::LLVM::LinkageAttr::get(
          f.getContext(), mlir::LLVM::Linkage::Private));
    }
  });

  return Status::OK();
}

StatusOr<mlir::ModuleOp> createMLIRModule(HloModule* module,
                                          mlir::MLIRContext& mlir_context,
                                          BufferAssignment* assignment) {
  LoadMLIRDialects(mlir_context);
  mlir::OpBuilder builder(&mlir_context);
  auto mlir_module = builder.create<mlir::ModuleOp>(builder.getUnknownLoc());
  TF_RETURN_IF_ERROR(ConvertHloToMlirHlo(mlir_module, module));

  // Add buffer mappings
  llvm::SmallVector<mlir::Attribute> operand_mapping;
  for (auto i : module->entry_computation()->parameter_instructions()) {
    auto slice = assignment->GetUniqueTopLevelSlice(i);
    operand_mapping.push_back(
        builder.getI32IntegerAttr(static_cast<int32_t>(slice->index())));
  }

  auto root_instr = module->entry_computation()->root_instruction();
  auto output_allocation = assignment->GetUniqueTopLevelOutputSlice();

  // Gather mappings to each element in the tuple if necessary
  llvm::SmallVector<mlir::Attribute> result_inner_mapping;
  if (output_allocation->allocation()->is_tuple()) {
    for (auto i : llvm::seq<int>(0, root_instr->shape().tuple_shapes_size())) {
      result_inner_mapping.push_back(mlir::IntegerAttr::get(
          mlir::IntegerType::get(&mlir_context, 64),
          assignment->GetUniqueSlice(root_instr, {i})->index()));
    }
  }

  auto result_mapping = builder.getI32IntegerAttr(
      static_cast<int32_t>(output_allocation->index()));
  mlir_module->walk([&](mlir::FuncOp f) {
    if (f.getSymName() == "main") {
      for (auto& p : llvm::enumerate(operand_mapping)) {
        f.setArgAttr(p.index(), "xla_framework.input_mapping", p.value());
      }
      f->setAttr("xla_framework.result_mapping", result_mapping);
    }

    if (output_allocation->allocation()->is_tuple()) {
      f->setAttr("xla_framework.result_inner_mapping",
                 mlir::ArrayAttr::get(f.getContext(), result_inner_mapping));
    }
  });
  return mlir_module;
}

struct ComputationToEmit {
  HloComputation* computation;

  // Are we emitting this computation with fast-math reassociation enabled?
  // We enable reassociation for reductions because it has a significant
  // performance impact.
  bool allow_reassociation;

  bool operator==(const ComputationToEmit& other) const {
    return computation == other.computation &&
           allow_reassociation == other.allow_reassociation;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ComputationToEmit& c) {
    return H::combine(std::move(h), c.computation, c.allow_reassociation);
  }
};

std::vector<ComputationToEmit> SubcomputationEmissionOrder(
    HloComputation* root) {
  absl::flat_hash_set<ComputationToEmit> visited;
  std::vector<ComputationToEmit> postorder;

  // agenda of (node, leave) pairs.
  std::stack<std::pair<ComputationToEmit, bool>> agenda;
  agenda.emplace(ComputationToEmit{root, false}, false);
  while (!agenda.empty()) {
    ComputationToEmit c;
    bool leave;
    std::tie(c, leave) = agenda.top();
    agenda.pop();

    if (leave) {
      postorder.push_back(c);
      continue;
    }

    if (visited.insert(c).second) {
      agenda.emplace(c, true);
      for (auto* instruction : c.computation->instructions()) {
        bool allow_reassociation =
            instruction->opcode() == HloOpcode::kAllReduce ||
            instruction->opcode() == HloOpcode::kReduce ||
            instruction->opcode() == HloOpcode::kReduceWindow;
        for (auto it = instruction->called_computations().rbegin();
             it != instruction->called_computations().rend(); ++it) {
          HloComputation* called_computation = *it;
          ComputationToEmit callee{
              called_computation, c.allow_reassociation || allow_reassociation};
          if (!visited.contains(callee)) {
            agenda.emplace(callee, false);
          }
        }
      }
    }
  }
  DCHECK(!postorder.empty() && postorder.back().computation == root);
  postorder.pop_back();
  return postorder;
}

}  // namespace

StatusOr<std::unique_ptr<Executable>> CpuCompiler::RunBackend(
    std::unique_ptr<HloModule> module, se::StreamExecutor* stream_exec,
    const CompileOptions& options) {
  VLOG(1) << "Compiling: " << module->name();
  XLA_SCOPED_LOGGING_TIMER(
      absl::StrFormat("Compiling [%s] for CPU using JIT", module->name()));
  std::string slow_compilation_msg =
      absl::StrCat("Compiling module ", module->name());
  auto slow_compile_alarm = SlowCompilationAlarm(slow_compilation_msg);

  absl::call_once(llvm_command_line_options_initialized,
                  &InitializeLLVMCommandLineOptions, module->config());

  ModuleHook pre_optimization_ir_hook;
  ModuleHook post_optimization_ir_hook;
  std::tie(pre_optimization_ir_hook, post_optimization_ir_hook) =
      GetIRModuleHooks(*module, user_pre_optimization_hook_,
                       user_post_optimization_hook_);

  // Compile must be thread-safe so create a new LLVM context for the module.
  mlir::MLIRContext mlir_context;
  LoadMLIRDialects(mlir_context);
  auto llvm_context = std::make_unique<llvm::LLVMContext>();
  auto llvm_module =
      absl::make_unique<llvm::Module>("__compute_module", *llvm_context);

  auto jit = SimpleOrcJIT::Create(
      CompilerTargetOptions(module->config()),
      CodeGenOptLevel(module->config()),
      options::OptimizeForSizeRequested(module->config()),
      module->config().debug_options().xla_llvm_disable_expensive_passes(),
      llvm_ir::GetCpuFastMathFlags(module->config()), pre_optimization_ir_hook,
      post_optimization_ir_hook,
      OrcJITPostCompilationHook::Create(module.get()));
  if (!jit) {
    return InternalError("Creating JIT failed: %s",
                         llvm::toString(jit.takeError()));
  }
  llvm_module->setDataLayout((*jit)->data_layout());
  llvm_module->setTargetTriple((*jit)->target_triple().getTriple());

  HloComputation* entry_computation = module->entry_computation();
  absl::flat_hash_map<const HloInstruction*, int64_t>
      instruction_to_profile_idx;
  absl::flat_hash_map<const HloComputation*, int64_t>
      computation_to_profile_idx;
  std::unique_ptr<HloProfileIndexMap> hlo_profile_index_map;
  std::unique_ptr<HloProfilePrinterData> hlo_profile_printer_data;
  if (module->config().hlo_profiling_enabled()) {
    TF_RETURN_IF_ERROR(CreateHloProfilingArtifacts(
        *module, &instruction_to_profile_idx, &computation_to_profile_idx,
        &hlo_profile_index_map, &hlo_profile_printer_data));
  }

  // Cache these flags here since we'll want to access them after the module's
  // ownership is std::moved.
  const bool embed_ir_in_executable =
      module->config().debug_options().xla_embed_ir_in_executable();

  // Select an order for emitting the HLO instructions for each
  // computation. Using this sequence enables tighter buffer liveness analysis
  // and reduced memory usage (as compared to using DependencyHloOrdering).
  TF_ASSIGN_OR_RETURN(HloSchedule schedule,
                      ScheduleModule(module.get(), BufferSizeBytesFunction(),
                                     ComputationSchedulerToModuleScheduler(
                                         DFSMemoryScheduler)));

  // Run buffer allocation on the HLO graph.
  TF_ASSIGN_OR_RETURN(
      std::unique_ptr<BufferAssignment> assignment,
      BufferAssigner::Run(module.get(),
                          absl::make_unique<SequentialHloOrdering>(schedule),
                          BufferSizeBytesFunction(), memory_alignment,
                          /*allocate_buffers_for_constants=*/true));
  DumpHloModuleIfEnabled(*module, *assignment, "cpu_after_optimizations");

  // Each computation is a single function.  Emit all embedded computations
  // before the entry computation. The order of computations returned from
  // GetEmbeddedComputations guarantees that a called computation occurs
  // before a caller computation.

  LLVMTargetMachineFeatures target_machine_features((*jit)->target_machine());
  IrEmitter ir_emitter(&mlir_context, *module, *assignment, llvm_module.get(),
                       std::move(instruction_to_profile_idx),
                       std::move(computation_to_profile_idx),
                       ModuleComputationsTransitivelyContainCustomCall(*module),
                       &target_machine_features,
#ifdef MEMORY_SANITIZER
                       /*emit_code_for_msan=*/true
#else
                       /*emit_code_for_msan=*/false
#endif
  );

  TF_RETURN_IF_ERROR(ir_emitter.EmitConstantGlobals());

  for (ComputationToEmit subcomputation :
       SubcomputationEmissionOrder(entry_computation)) {
    if (subcomputation.computation->IsFusionComputation()) {
      continue;
    }
    TF_RETURN_IF_ERROR(
        ir_emitter
            .EmitComputation(
                subcomputation.computation, subcomputation.computation->name(),
                /*is_top_level_computation=*/false,
                schedule.sequence(subcomputation.computation).instructions(),
                subcomputation.allow_reassociation)
            .status());
  }
  std::string function_name_prefix = entry_computation->name().empty()
                                         ? "__compute"
                                         : entry_computation->name();
  TF_ASSIGN_OR_RETURN(llvm::Function * entry_function,
                      ir_emitter.EmitComputation(
                          entry_computation, function_name_prefix,
                          /*is_top_level_computation=*/true,
                          schedule.sequence(entry_computation).instructions(),
                          /*allow_reassociation=*/false));

  std::string function_name = [&]() {
    llvm::SmallVector<char, 40> function_name_vector;
    llvm::Mangler::getNameWithPrefix(
        function_name_vector, entry_function->getName(), (*jit)->data_layout());
    return std::string(function_name_vector.begin(),
                       function_name_vector.end());
  }();

  std::string ir_module_string;
  if (embed_ir_in_executable) {
    ir_module_string = llvm_ir::DumpModuleToString(*llvm_module);
  }

  TF_RETURN_IF_ERROR(VerifyLlvmModule(*llvm_module));

  // JIT compile the LLVM IR module to in-memory machine code.
  llvm::orc::ThreadSafeModule thread_safe_module(std::move(llvm_module),
                                                 std::move(llvm_context));
  cantFail((*jit)->AddModule(std::move(thread_safe_module)));

  auto cpu_executable = absl::make_unique<CpuExecutable>(
      std::move(*jit), std::move(assignment), std::move(module), function_name,
      std::move(hlo_profile_printer_data), std::move(hlo_profile_index_map));

  if (embed_ir_in_executable) {
    cpu_executable->set_ir_module_string(ir_module_string);
  }

  // Dump computation proto state and buffer assignment for debug and test, if
  // dump or embed_ir_in_executable is enabled.
  if (embed_ir_in_executable ||
      DumpingEnabledForHloModule(cpu_executable->module())) {
    auto hlo_proto = absl::make_unique<HloProto>();
    if (hlo_proto_) {
      *hlo_proto = *hlo_proto_;
    } else {
      *hlo_proto->mutable_hlo_module() = cpu_executable->module().ToProto();
    }
    *hlo_proto->mutable_buffer_assignment() =
        cpu_executable->buffer_assignment().ToProto();
    cpu_executable->set_hlo_proto(std::move(hlo_proto));
  }

  cpu_executable->set_debug_info(
      cpu_executable->buffer_assignment().GetStats().ToString());
  VLOG(1) << "Compilation finished";
  return std::unique_ptr<Executable>(std::move(cpu_executable));
}

StatusOr<std::vector<std::unique_ptr<AotCompilationResult>>>
CpuCompiler::CompileAheadOfTime(std::unique_ptr<HloModuleGroup> module_group,
                                const AotCompilationOptions& aot_options) {
  TF_RET_CHECK(!module_group->empty());
  std::vector<std::unique_ptr<HloModule>> modules =
      module_group->ConsumeModules();

  absl::call_once(llvm_command_line_options_initialized,
                  &InitializeLLVMCommandLineOptions, modules[0]->config());

  // We can pass just one llvm::TargetOptions when we compile the LLVM module,
  // so we bail if the configs have conflicting flags. At the moment, the only
  // flags that need to be consistent are for fast-math.
  for (const auto& fn_and_name :
       {std::make_pair(&DebugOptions::xla_cpu_enable_fast_math,
                       "xla_cpu_enable_fast_math"),
        std::make_pair(&DebugOptions::xla_cpu_fast_math_honor_infs,
                       "xla_cpu_fast_math_honor_infs"),
        std::make_pair(&DebugOptions::xla_cpu_fast_math_honor_nans,
                       "xla_cpu_fast_math_honor_nans")}) {
    // This only works because each of the method pointers above returns a bool.
    // Otherwise we'd have to do some template magic.
    const auto& field_method_ptr = fn_and_name.first;
    const auto& field_name = fn_and_name.second;
    bool first_module_val =
        (modules[0]->config().debug_options().*field_method_ptr)();
    for (int64_t i = 0; i < modules.size(); ++i) {
      bool cur_module_val =
          (modules[i]->config().debug_options().*field_method_ptr)();
      if (first_module_val != cur_module_val) {
        return InvalidArgument(
            "All HLO module configs must have the same value for %s, but "
            "module 0 and %d have different values (%d vs %d).",
            field_name, i, first_module_val, cur_module_val);
      }
    }
  }

  if (aot_options.PlatformId() != se::host::kHostPlatformId) {
    return InvalidArgument("Incompatible AOT compilation platform");
  }
  const CpuAotCompilationOptions& options =
      static_cast<const CpuAotCompilationOptions&>(aot_options);
  llvm::Triple triple(llvm::Triple::normalize(options.triple()));
  std::string error;
  const llvm::Target* target =
      llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
  if (target == nullptr) {
    return InternalError("TargetRegistry::lookupTarget failed: %s", error);
  }

  llvm::Reloc::Model reloc_model = llvm::Reloc::Static;
  llvm::PICLevel::Level pic_level = llvm::PICLevel::NotPIC;
  llvm::PIELevel::Level pie_level = llvm::PIELevel::Default;
  switch (options.relocation_model()) {
    case CpuAotCompilationOptions::RelocationModel::Static:
      reloc_model = llvm::Reloc::Static;
      pic_level = llvm::PICLevel::NotPIC;
      pie_level = llvm::PIELevel::Default;
      break;
    case CpuAotCompilationOptions::RelocationModel::SmallPic:
      reloc_model = llvm::Reloc::PIC_;
      pic_level = llvm::PICLevel::SmallPIC;
      pie_level = llvm::PIELevel::Default;
      break;
    case CpuAotCompilationOptions::RelocationModel::BigPic:
      reloc_model = llvm::Reloc::PIC_;
      pic_level = llvm::PICLevel::BigPIC;
      pie_level = llvm::PIELevel::Default;
      break;
    case CpuAotCompilationOptions::RelocationModel::SmallPie:
      reloc_model = llvm::Reloc::PIC_;
      pic_level = llvm::PICLevel::SmallPIC;
      pie_level = llvm::PIELevel::Small;
      break;
    case CpuAotCompilationOptions::RelocationModel::BigPie:
      reloc_model = llvm::Reloc::PIC_;
      pic_level = llvm::PICLevel::BigPIC;
      pie_level = llvm::PIELevel::Large;
      break;
  }
  llvm::CodeGenOpt::Level opt_level = CodeGenOptLevel(modules[0]->config());
  std::unique_ptr<llvm::TargetMachine> target_machine =
      absl::WrapUnique(target->createTargetMachine(
          triple.getTriple(), options.cpu_name(), options.features(),
          CompilerTargetOptions(modules[0]->config()), reloc_model, llvm::None,
          opt_level));

  // Compile must be thread-safe so create a new LLVM context for the module.
  mlir::MLIRContext mlir_context;
  LoadMLIRDialects(mlir_context);
  llvm::LLVMContext llvm_context;
  std::unique_ptr<llvm::Module> llvm_module;

  std::vector<std::unique_ptr<AotCompilationResult>> results;
  for (size_t i = 0; i < modules.size(); ++i) {
    HloModule* module = modules[i].get();
    VLOG(1) << "Compiling ahead-of-time: " << module->name();

    TF_RETURN_IF_ERROR(
        RunHloPasses(module, /*is_aot_compile=*/true, target_machine.get(),
                     /*is_mlir_compile=*/options.use_mlir_hlo_lowering()));

    TF_ASSIGN_OR_RETURN(HloSchedule schedule,
                        ScheduleModule(module, BufferSizeBytesFunction()));

    // Run buffer analysis on the HLO graph. This analysis figures out which
    // temporary buffers are required to run the computation.
    TF_ASSIGN_OR_RETURN(
        std::unique_ptr<BufferAssignment> assignment,
        BufferAssigner::Run(module,
                            absl::make_unique<SequentialHloOrdering>(schedule),
                            BufferSizeBytesFunction(), memory_alignment,
                            /*allocate_buffers_for_constants=*/true));
    // BufferAssignment::ToString() includes a header, so no need for us to
    // print one ourselves.
    if (DumpingEnabledForHloModule(*module)) {
      DumpToFileInDirOrStdout(*module, "", "buffer_assignment",
                              assignment->ToString());
    }
    DumpHloModuleIfEnabled(*module, *assignment, "cpu_after_optimizations");

    absl::flat_hash_map<const HloInstruction*, int64_t>
        instruction_to_profile_idx;
    absl::flat_hash_map<const HloComputation*, int64_t>
        computation_to_profile_idx;
    std::unique_ptr<HloProfileIndexMap> hlo_profile_index_map;
    std::unique_ptr<HloProfilePrinterData> hlo_profile_printer_data;

    if (module->config().hlo_profiling_enabled()) {
      TF_RETURN_IF_ERROR(CreateHloProfilingArtifacts(
          *module, &instruction_to_profile_idx, &computation_to_profile_idx,
          &hlo_profile_index_map, &hlo_profile_printer_data));
    }

    LLVMTargetMachineFeatures target_machine_features(target_machine.get());
    std::vector<BufferInfo> buffer_infos =
        CreateBufferInfosFromBufferAssignment(*assignment);
    HloComputation* computation = module->entry_computation();

    if (UseMlirHloLowering(options.use_mlir_hlo_lowering(), module)) {
      TF_ASSIGN_OR_RETURN(
          auto mlir_module,
          createMLIRModule(module, mlir_context, assignment.get()));
      TF_RETURN_IF_ERROR(LowerMLIRModule(mlir_module, mlir_context));

      llvm::cast<mlir::LLVM::LLVMFuncOp>(
          mlir_module.lookupSymbol("main_xla_framework"))
          .setName(options.entry_point_name());

      llvm_module = mlir::translateModuleToLLVMIR(mlir_module, llvm_context);
      // Set missing information
      llvm_module->setDataLayout(target_machine->createDataLayout());
      llvm_module->setTargetTriple(triple.getTriple());
      if (pic_level != llvm::PICLevel::NotPIC) {
        llvm_module->setPICLevel(pic_level);
      }
      if (pie_level != llvm::PIELevel::Default) {
        llvm_module->setPIELevel(pie_level);
      }
    } else {
      // Set required information before emitting IR
      llvm_module =
          std::make_unique<llvm::Module>("__compute_module", llvm_context);
      llvm_module->setDataLayout(target_machine->createDataLayout());
      llvm_module->setTargetTriple(triple.getTriple());
      if (pic_level != llvm::PICLevel::NotPIC) {
        llvm_module->setPICLevel(pic_level);
      }
      if (pie_level != llvm::PIELevel::Default) {
        llvm_module->setPIELevel(pie_level);
      }
      IrEmitter ir_emitter(
          &mlir_context, *module, *assignment, llvm_module.get(),
          std::move(instruction_to_profile_idx),
          std::move(computation_to_profile_idx),
          ModuleComputationsTransitivelyContainCustomCall(*module),
          &target_machine_features,
          // TODO(b/66051036): Run full msan for AOT.
          /*emit_code_for_msan=*/false);

      TF_RETURN_IF_ERROR(ir_emitter.EmitConstantGlobals());

      for (ComputationToEmit subcomputation :
           SubcomputationEmissionOrder(computation)) {
        if (subcomputation.computation->IsFusionComputation()) {
          continue;
        }
        TF_RETURN_IF_ERROR(
            ir_emitter
                .EmitComputation(subcomputation.computation,
                                 subcomputation.computation->name(),
                                 /*is_top_level_computation=*/false,
                                 schedule.sequence(subcomputation.computation)
                                     .instructions(),
                                 subcomputation.allow_reassociation)
                .status());
      }
      const std::string& entry_point_name = options.entry_point_name();
      TF_ASSIGN_OR_RETURN(llvm::Function * entry_function,
                          ir_emitter.EmitComputation(
                              computation, entry_point_name,
                              /*is_top_level_computation=*/true,
                              schedule.sequence(computation).instructions(),
                              /*allow_reassociation=*/false));

      CHECK(entry_function->getName() == entry_point_name);
    }

    ModuleHook pre_optimization_ir_hook;
    ModuleHook post_optimization_ir_hook;
    std::tie(pre_optimization_ir_hook, post_optimization_ir_hook) =
        GetIRModuleHooks(*module, user_pre_optimization_hook_,
                         user_post_optimization_hook_);

    // Run the LLVM verifier over the unoptimized LLVM IR.  If it fails, run
    // the pre-optimization IR dump hook before returning.
    {
      Status verify_status = VerifyLlvmModule(*llvm_module);
      if (!verify_status.ok() && pre_optimization_ir_hook) {
        pre_optimization_ir_hook(*llvm_module);
      }
      TF_RETURN_IF_ERROR(verify_status);
    }

    auto post_codegen_hook = [&](const llvm::object::ObjectFile& obj_file) {
      if (!DumpingEnabledForHloModule(*module)) {
        return;
      }
      DumpToFileInDir(*module, /*file_prefix=*/"", /*file_suffix=*/"o",
                      absl::string_view(obj_file.getData().data(),
                                        obj_file.getData().size()));
    };
    CompilerFunctor compiler_functor(
        target_machine.get(), opt_level,
        options::OptimizeForSizeRequested(module->config()),
        module->config().debug_options().xla_llvm_disable_expensive_passes(),
        llvm_ir::GetCpuFastMathFlags(module->config()),
        pre_optimization_ir_hook, post_optimization_ir_hook, post_codegen_hook);
    std::unique_ptr<llvm::MemoryBuffer> object_file =
        cantFail(compiler_functor(*llvm_module));
    ObjectFileData object_file_data(object_file->getBufferStart(),
                                    object_file->getBufferEnd());

    TF_ASSIGN_OR_RETURN(const BufferAllocation::Slice result_slice,
                        assignment->GetUniqueTopLevelOutputSlice());

    results.emplace_back(absl::make_unique<CpuAotCompilationResult>(
        std::move(object_file_data), std::move(buffer_infos),
        result_slice.index(), std::move(hlo_profile_printer_data)));
  }

  VLOG(1) << "Compilation finished";
  return std::move(results);
}

se::Platform::Id CpuCompiler::PlatformId() const {
  return se::host::kHostPlatformId;
}

HloCostAnalysis::ShapeSizeFunction CpuCompiler::ShapeSizeBytesFunction() const {
  return CpuExecutable::ShapeSizeBytes;
}

}  // namespace cpu
}  // namespace xla

static bool InitModule() {
  xla::Compiler::RegisterCompilerFactory(
      stream_executor::host::kHostPlatformId,
      []() { return absl::make_unique<xla::cpu::CpuCompiler>(); });
  return true;
}
static bool module_initialized = InitModule();
