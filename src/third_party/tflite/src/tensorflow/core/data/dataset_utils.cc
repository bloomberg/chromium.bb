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

#include "tensorflow/core/data/dataset_utils.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op_def_builder.h"
#include "tensorflow/core/framework/op_def_util.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/framework/tensor_util.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/graph/graph_def_builder.h"
#include "tensorflow/core/lib/core/blocking_counter.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/lib/strings/proto_serialization.h"
#include "tensorflow/core/platform/host_info.h"
#include "tensorflow/core/platform/regexp.h"
#include "tensorflow/core/util/determinism.h"
#include "tensorflow/core/util/work_sharder.h"

namespace tensorflow {
namespace data {
namespace {

constexpr char kOutputSize[] = "output_size";
constexpr char kCode[] = "code";
constexpr char kMessage[] = "msg";
constexpr char kOutput[] = "output";

static mutex* get_dataset_experiment_registry_lock() {
  static mutex dataset_experiment_registry_lock(LINKER_INITIALIZED);
  return &dataset_experiment_registry_lock;
}

static absl::flat_hash_map<string, int64_t>* get_dataset_experiments() {
  static absl::flat_hash_map<string, int64_t>* experiments =
      new absl::flat_hash_map<string, int64_t>;
  return experiments;
}

// Use "Opt" suffix so that they are not confused with the enums in Options
// proto.
constexpr char kMapAndBatchFusionOpt[] = "map_and_batch_fusion";
constexpr char kNoopEliminationOpt[] = "noop_elimination";
constexpr char kMapParallelizationOpt[] = "map_parallelization";
constexpr char kShuffleAndRepeatFusionOpt[] = "shuffle_and_repeat_fusion";
constexpr char kFilterFusionOpt[] = "filter_fusion";
constexpr char kMapAndFilterFusionOpt[] = "map_and_filter_fusion";
constexpr char kMapFusionOpt[] = "map_fusion";
constexpr char kParallelBatchOpt[] = "parallel_batch";
constexpr char kAutotuneBufferSizesOpt[] = "autotune_buffer_sizes";
constexpr char kDisablePrefetchLegacyAutotuneOpt[] =
    "disable_prefetch_legacy_autotune";
constexpr char kMakeSloppyOpt[] = "make_sloppy";
constexpr char kUseChooseFastestOpt[] = "use_choose_fastest";
constexpr char kBatchParallelizationOpt[] = "batch_parallelization";
constexpr char kEnableGradientDescentOpt[] = "enable_gradient_descent";
constexpr char kInjectPrefetchOpt[] = "inject_prefetch";
constexpr char kInjectPrefetchEligibleOpt[] = "inject_prefetch_eligible";
constexpr char kAutotuneOpt[] = "autotune";
constexpr char kSlackOpt[] = "slack";
constexpr char kSlackPeriodOpt[] = "slack_period";
constexpr char kMakeDeterministicOpt[] = "make_deterministic";

void DefaultOptimizationGraphRewrites(
    const Options& options, absl::flat_hash_set<tstring>* optimization_enabled,
    absl::flat_hash_set<tstring>* optimization_disabled,
    absl::flat_hash_set<tstring>* optimization_default) {
  const auto& optimization_options = options.optimization_options();
  if (optimization_options.optional_apply_default_optimizations_case() !=
          OptimizationOptions::kApplyDefaultOptimizations ||
      optimization_options.apply_default_optimizations()) {
    if (optimization_options.optional_map_and_batch_fusion_case() !=
        OptimizationOptions::kMapAndBatchFusion) {
      optimization_default->insert(kMapAndBatchFusionOpt);
    }
    if (optimization_options.optional_noop_elimination_case() !=
        OptimizationOptions::kNoopElimination) {
      optimization_default->insert(kNoopEliminationOpt);
    }
    if (optimization_options.optional_map_parallelization_case() !=
        OptimizationOptions::kMapParallelization) {
      optimization_default->insert(kMapParallelizationOpt);
    }
    if (optimization_options.optional_shuffle_and_repeat_fusion_case() !=
        OptimizationOptions::kShuffleAndRepeatFusion) {
      optimization_default->insert(kShuffleAndRepeatFusionOpt);
    }
    if (optimization_options.optional_parallel_batch_case() !=
        OptimizationOptions::kParallelBatch) {
      optimization_default->insert(kParallelBatchOpt);
    }
  }
  if (OpDeterminismRequired()) {
    optimization_enabled->insert(kMakeDeterministicOpt);
  }
  if (optimization_options.optional_filter_fusion_case() ==
      OptimizationOptions::kFilterFusion) {
    if (optimization_options.filter_fusion()) {
      optimization_enabled->insert(kFilterFusionOpt);
    } else {
      optimization_disabled->insert(kFilterFusionOpt);
    }
  }
  if (optimization_options.optional_map_and_batch_fusion_case() ==
      OptimizationOptions::kMapAndBatchFusion) {
    if (optimization_options.map_and_batch_fusion()) {
      optimization_enabled->insert(kMapAndBatchFusionOpt);
    } else {
      optimization_disabled->insert(kMapAndBatchFusionOpt);
    }
  }
  if (optimization_options.optional_map_and_filter_fusion_case() ==
      OptimizationOptions::kMapAndFilterFusion) {
    if (optimization_options.map_and_filter_fusion()) {
      optimization_enabled->insert(kMapAndFilterFusionOpt);
    } else {
      optimization_disabled->insert(kMapAndFilterFusionOpt);
    }
  }
  if (optimization_options.optional_map_parallelization_case() ==
      OptimizationOptions::kMapParallelization) {
    if (optimization_options.map_parallelization()) {
      optimization_enabled->insert(kMapParallelizationOpt);
    } else {
      optimization_disabled->insert(kMapParallelizationOpt);
    }
  }
  if (optimization_options.optional_map_fusion_case() ==
      OptimizationOptions::kMapFusion) {
    if (optimization_options.map_fusion()) {
      optimization_enabled->insert(kMapFusionOpt);
    } else {
      optimization_disabled->insert(kMapFusionOpt);
    }
  }
  if (optimization_options.optional_noop_elimination_case() ==
      OptimizationOptions::kNoopElimination) {
    if (optimization_options.noop_elimination()) {
      optimization_enabled->insert(kNoopEliminationOpt);
    } else {
      optimization_disabled->insert(kNoopEliminationOpt);
    }
  }
  if (optimization_options.optional_parallel_batch_case() ==
      OptimizationOptions::kParallelBatch) {
    if (optimization_options.parallel_batch()) {
      optimization_enabled->insert(kParallelBatchOpt);
    } else {
      optimization_disabled->insert(kParallelBatchOpt);
    }
  }
  if (optimization_options.optional_shuffle_and_repeat_fusion_case() ==
      OptimizationOptions::kShuffleAndRepeatFusion) {
    if (optimization_options.shuffle_and_repeat_fusion()) {
      optimization_enabled->insert(kShuffleAndRepeatFusionOpt);
    } else {
      optimization_disabled->insert(kShuffleAndRepeatFusionOpt);
    }
  }
}

// Returns whether an op has been allowlisted as stateless. Uses a heuristic to
// allowlist source dataset ops which have been marked stateful due to
// b/65524810. Also looks up the `op_def->name` in the global
// `AllowlistedStatefulOpRegistry`.
bool IsOpAllowlisted(const OpDef* op_def) {
  return (op_def->output_arg_size() == 1 &&
          op_def->output_arg(0).type() == DT_VARIANT &&
          (absl::EndsWith(op_def->name(), "Dataset") ||
           absl::EndsWith(op_def->name(), "DatasetV2"))) ||
         AllowlistedStatefulOpRegistry::Global()->Contains(op_def->name());
}

}  // namespace

std::pair<int64_t, int64_t> MaybeOverrideSeeds(
    std::pair<int64_t, int64_t> seeds) {
  if (seeds.first == 0 && seeds.second == 0) {
    return {random::New64(), random::New64()};
  }
  return seeds;
}

Status VerifyTypeMatch(const DataType& expected, const DataType& received,
                       int index) {
  if (expected != received) {
    return errors::InvalidArgument("Data type mismatch at component ", index,
                                   ": expected ", DataTypeString(expected),
                                   " but got ", DataTypeString(received), ".");
  }
  return Status::OK();
}

Status VerifyTypesMatch(const DataTypeVector& expected,
                        const DataTypeVector& received) {
  if (expected.size() != received.size()) {
    return errors::InvalidArgument(
        "Number of components does not match: expected ", expected.size(),
        " types but got ", received.size(), ".");
  }
  for (size_t i = 0; i < expected.size(); ++i) {
    TF_RETURN_IF_ERROR(VerifyTypeMatch(expected[i], received[i], i));
  }
  return Status::OK();
}

Status VerifyTypesMatch(const DataTypeVector& expected,
                        const std::vector<Tensor>& received) {
  if (expected.size() != received.size()) {
    return errors::InvalidArgument(
        "Number of components does not match: expected ", expected.size(),
        " types but got ", received.size(), ".");
  }
  for (size_t i = 0; i < expected.size(); ++i) {
    TF_RETURN_IF_ERROR(VerifyTypeMatch(expected[i], received[i].dtype(), i));
  }
  return Status::OK();
}

Status VerifyShapeCompatible(const PartialTensorShape& expected,
                             const PartialTensorShape& received, int index) {
  if (!expected.IsCompatibleWith(received)) {
    return errors::InvalidArgument("Incompatible shapes at component ", index,
                                   ": expected ", expected.DebugString(),
                                   " but got ", received.DebugString(), ".");
  }
  return Status::OK();
}

Status VerifyShapesCompatible(const std::vector<PartialTensorShape>& expected,
                              const std::vector<PartialTensorShape>& received) {
  if (expected.size() != received.size()) {
    return errors::InvalidArgument(
        "Number of components does not match: expected ", expected.size(),
        " shapes but got ", received.size(), ".");
  }
  for (size_t i = 0; i < expected.size(); ++i) {
    TF_RETURN_IF_ERROR(VerifyShapeCompatible(expected[i], received[i], i));
  }

  return Status::OK();
}

Status VerifyShapesCompatible(const std::vector<PartialTensorShape>& expected,
                              const std::vector<Tensor>& received) {
  if (expected.size() != received.size()) {
    return errors::InvalidArgument(
        "Number of components does not match: expected ", expected.size(),
        " shapes but got ", received.size(), ".");
  }
  for (size_t i = 0; i < expected.size(); ++i) {
    TF_RETURN_IF_ERROR(
        VerifyShapeCompatible(expected[i], received[i].shape(), i));
  }

  return Status::OK();
}

Status AddToFunctionLibrary(FunctionLibraryDefinition* base,
                            const FunctionLibraryDefinition& to_add) {
  for (const auto& fn : to_add.ListFunctionNames()) {
    if (auto found = base->Find(fn)) {
      if (!OpDefEqual(found->signature(), to_add.Find(fn)->signature())) {
        return errors::InvalidArgument("Cannot add function '", fn,
                                       "' because a different function with "
                                       "the same signature already exists.");
      }
      TF_RETURN_IF_ERROR(base->RemoveFunction(fn));
    }
  }
  return base->AddLibrary(to_add);
}

Status AddToFunctionLibrary(FunctionLibraryDefinition* base,
                            const FunctionDefLibrary& to_add) {
  for (const auto& fd : to_add.function()) {
    if (auto found = base->Find(fd.signature().name())) {
      if (!OpDefEqual(found->signature(), fd.signature())) {
        return errors::InvalidArgument("Cannot add function '",
                                       fd.signature().name(),
                                       "' because a different function with "
                                       "the same signature already exists.");
      }
      TF_RETURN_IF_ERROR(base->RemoveFunction(fd.signature().name()));
    }
  }
  return base->AddLibrary(to_add);
}

Status IsFunctionStateful(const FunctionLibraryDefinition& library,
                          const FunctionDef& function_def) {
  if (!function_def.signature().is_stateful()) {
    return Status::OK();
  }

  for (const NodeDef& node_def : function_def.node_def()) {
    TF_RETURN_IF_ERROR(IsNodeStateful(library, node_def));
  }
  return Status::OK();
}

Status IsNodeStateful(const FunctionLibraryDefinition& library,
                      const NodeDef& node) {
  const OpDef* op_def;

  // TODO(jsimsa): Fix C++ unit tests so that we do not have to ignore
  // `LookUpOpDef` errors here.
  if (!OpRegistry::Global()->LookUpOpDef(node.op(), &op_def).ok() ||
      IsOpAllowlisted(op_def) || !op_def->is_stateful() ||
      op_def->name() == "Assert") {
    return Status::OK();
  }

  if (op_def->name() == "If") {
    const FunctionDef* then_func =
        library.Find(node.attr().at("then_branch").func().name());
    const FunctionDef* else_func =
        library.Find(node.attr().at("else_branch").func().name());
    if (then_func != nullptr) {
      TF_RETURN_IF_ERROR(IsFunctionStateful(library, *then_func));
    }
    if (else_func != nullptr) {
      TF_RETURN_IF_ERROR(IsFunctionStateful(library, *else_func));
    }
    return Status::OK();
  }

  if (op_def->name() == "While") {
    const FunctionDef* cond_func =
        library.Find(node.attr().at("cond").func().name());
    const FunctionDef* body_func =
        library.Find(node.attr().at("body").func().name());
    if (cond_func != nullptr) {
      TF_RETURN_IF_ERROR(IsFunctionStateful(library, *cond_func));
    }
    if (body_func != nullptr) {
      TF_RETURN_IF_ERROR(IsFunctionStateful(library, *body_func));
    }
    return Status::OK();
  }

  return errors::FailedPrecondition(op_def->name(), " is stateful.");
}

std::function<void(std::function<void()>)> RunnerWithMaxParallelism(
    std::function<void(std::function<void()>)> runner, int max_parallelism) {
  return std::bind(
      [max_parallelism](
          // Note: `runner` is a const reference to avoid copying it.
          const std::function<void(std::function<void()>)>& runner,
          std::function<void()> fn) {
        std::function<void()> scoped_fn = std::bind(
            [max_parallelism](const std::function<void()>& fn) {
              ScopedPerThreadMaxParallelism scope(max_parallelism);
              fn();
            },
            std::move(fn));
        runner(std::move(scoped_fn));
      },
      std::move(runner), std::placeholders::_1);
}

Status DeterminismPolicy::FromString(const std::string& s,
                                     DeterminismPolicy* out) {
  DeterminismPolicy::Type type;
  if (s == DeterminismPolicy::kDeterministic) {
    type = DeterminismPolicy::Type::kDeterministic;
  } else if (s == DeterminismPolicy::kNondeterministic) {
    type = DeterminismPolicy::Type::kNondeterministic;
  } else if (s == DeterminismPolicy::kDefault) {
    type = DeterminismPolicy::Type::kDefault;
  } else {
    return errors::InvalidArgument("Unrecognized determinism policy: ", s);
  }
  *out = DeterminismPolicy(type);
  return Status::OK();
}

DeterminismPolicy::DeterminismPolicy(bool is_deterministic) {
  if (is_deterministic) {
    determinism_ = DeterminismPolicy::Type::kDeterministic;
  } else {
    determinism_ = DeterminismPolicy::Type::kNondeterministic;
  }
}

std::string DeterminismPolicy::String() const {
  switch (determinism_) {
    case DeterminismPolicy::Type::kDeterministic:
      return DeterminismPolicy::kDeterministic;
    case DeterminismPolicy::Type::kNondeterministic:
      return DeterminismPolicy::kNondeterministic;
    case DeterminismPolicy::Type::kDefault:
      return DeterminismPolicy::kDefault;
    default:
      LOG(ERROR) << "Unrecognized determinism value";
      return "Unrecognized";
  }
}

bool MatchesAnyVersion(StringPiece op_prefix, StringPiece op_to_match) {
  if (!absl::StartsWith(op_to_match, op_prefix)) {
    return false;
  }
  if (op_to_match.length() == op_prefix.length()) {
    return true;
  }
  size_t index = op_to_match.length() - 1;
  while (isdigit(op_to_match[index])) {
    index--;
  }
  return (op_to_match[index] == 'V') && (op_prefix.length() == index);
}

absl::flat_hash_set<string> GetExperiments() {
  return GetExperiments(port::JobName(),
                        [](const tstring& str) { return Hash64(str); });
}

absl::flat_hash_set<string> GetExperiments(
    const string& job_name, std::function<uint64(const string&)> hash_func) {
  absl::flat_hash_set<string> experiments;

  if (job_name.empty()) {
    return experiments;
  }

  // Parse the opt-in and opt-out settings.
  const char* opt_ins_raw_cs = std::getenv("TF_DATA_EXPERIMENT_OPT_IN");
  const char* opt_outs_raw_cs = std::getenv("TF_DATA_EXPERIMENT_OPT_OUT");
  string opt_ins_raw;
  if (opt_ins_raw_cs != nullptr) {
    opt_ins_raw = string(opt_ins_raw_cs);
  }
  string opt_outs_raw;
  if (opt_outs_raw_cs != nullptr) {
    opt_outs_raw = string(opt_outs_raw_cs);
  }

  // Identify opted out experiments.
  absl::flat_hash_map<string, int64_t> live_experiments =
      DatasetExperimentRegistry::Experiments();
  absl::flat_hash_set<string> opt_outs;
  if (opt_outs_raw == "all") {
    for (const auto& pair : live_experiments) {
      opt_outs.insert(pair.first);
    }
  } else {
    for (const auto& experiment :
         str_util::Split(opt_outs_raw, ',', str_util::SkipEmpty())) {
      opt_outs.insert(experiment);
    }
  }

  // Include opted in experiments unless they are opted out.
  if (opt_ins_raw == "all") {
    for (const auto& pair : live_experiments) {
      auto experiment = pair.first;
      if (!opt_outs.contains(experiment)) {
        experiments.insert(experiment);
      }
    }
  } else {
    for (const auto& experiment :
         str_util::Split(opt_ins_raw, ',', str_util::SkipEmpty())) {
      if (!opt_outs.contains(experiment)) {
        experiments.insert(experiment);
      }
    }
  }

  // Stochastically include live experiments unless they are opted out.
  for (const auto& pair : live_experiments) {
    auto& experiment = pair.first;
    if ((hash_func(strings::StrCat(job_name, experiment)) % 100 <
         pair.second) &&
        !opt_outs.contains(experiment)) {
      experiments.insert(experiment);
    }
  }

  return experiments;
}

void LogAndRecordExperiments(const absl::flat_hash_set<string>& experiments) {
  if (!experiments.empty()) {
    constexpr float TEN_MINUTES = 60.0 * 10.0;
    LOG_EVERY_N_SEC(INFO, TEN_MINUTES)
        << "The input pipeline is subject to the following tf.data experiments:"
        << " " << absl::StrJoin(experiments, ", ") << ". "
        << "See `go/tf-data-experiments` for more details.";
  }
  for (auto& experiment : experiments) {
    metrics::RecordTFDataExperiment(experiment);
  }
}

void GetOptimizations(const Options& options,
                      absl::flat_hash_set<tstring>* optimizations_enabled,
                      absl::flat_hash_set<tstring>* optimizations_disabled,
                      absl::flat_hash_set<tstring>* optimizations_default) {
  DefaultOptimizationGraphRewrites(options, optimizations_enabled,
                                   optimizations_disabled,
                                   optimizations_default);
  if (!OpDeterminismRequired() &&
      options.optional_deterministic_case() == Options::kDeterministic &&
      !options.deterministic()) {
    optimizations_enabled->insert(kMakeSloppyOpt);
  }
  if (options.optional_slack_case() == Options::kSlack) {
    if (options.slack()) {
      optimizations_enabled->insert(kSlackOpt);
    } else {
      optimizations_disabled->insert(kSlackOpt);
    }
  }
}

Tensor MaybeCopySubSlice(const Tensor& tensor, int64 index) {
  Tensor slice = tensor.SubSlice(index);
  if (slice.IsAligned()) {
    return slice;
  } else {
    return tensorflow::tensor::DeepCopy(slice);
  }
}

void StripDevicePlacement(FunctionDefLibrary* library) {
  for (auto& function : (*library->mutable_function())) {
    for (auto& node : (*function.mutable_node_def())) {
      if (!node.device().empty()) {
        *node.mutable_device() = "";
      }
    }
  }
}

Status CopyPartialBatch(int64_t num_elements, const Tensor& value,
                        Tensor* output) {
  switch (value.dtype()) {
#define HANDLE_TYPE(type)                                         \
  case DataTypeToEnum<type>::value: {                             \
    auto output_t = output->flat_outer_dims<type>();              \
    auto value_t = value.flat_outer_dims<type>();                 \
    for (size_t i = 0; i < num_elements; i++) {                   \
      output_t.template chip<0>(i) = value_t.template chip<0>(i); \
    }                                                             \
    return Status::OK();                                          \
  }
    TF_CALL_DATASET_TYPES(HANDLE_TYPE);
#undef HANDLE_TYPE
    default:
      return errors::InvalidArgument("Unsupported data type: ",
                                     DataTypeString(value.dtype()));
  }
  return Status::OK();
}

Status ReadBatch(IteratorContext* ctx, IteratorStateReader* reader,
                 int64_t batch_size, const string& iterator_prefix,
                 const string& batch_prefix, std::vector<Tensor>* batch) {
  int64_t output_size;
  TF_RETURN_IF_ERROR(reader->ReadScalar(
      FullName(iterator_prefix,
               strings::StrCat(batch_prefix, "_", kOutputSize)),
      &output_size));
  batch->reserve(output_size);
  for (int i = 0; i < output_size; i++) {
    Tensor t;
    TF_RETURN_IF_ERROR(
        reader->ReadTensor(ctx->flr(), FullName(iterator_prefix, batch_prefix),
                           strings::StrCat(kOutput, "_", i), &t));
    // If the batch was not full, we may have stored only the relevant slice.
    // Since tensors in `BatchResult.output` are expected to have the leading
    // dimension of size batch_size, we build a larger tensor and copy the slice
    // read from the checkpoint into it.
    if (t.dim_size(0) < batch_size) {
      TensorShape component_shape(t.shape());
      component_shape.set_dim(0, batch_size);
      AllocatorAttributes attr;
      attr.set_gpu_compatible(true);
      Tensor new_t(ctx->allocator(attr), t.dtype(), component_shape);
      TF_RETURN_IF_ERROR(CopyPartialBatch(t.dim_size(0), t, &new_t));
      batch->emplace_back(std::move(new_t));
    } else {
      batch->emplace_back(std::move(t));
    }
  }
  return Status::OK();
}

Status WriteBatch(int64_t batch_size, int64_t num_elements,
                  const string& iterator_prefix, const string& batch_prefix,
                  IteratorStateWriter* writer, std::vector<Tensor>* batch) {
  TF_RETURN_IF_ERROR(writer->WriteScalar(
      FullName(iterator_prefix,
               strings::StrCat(batch_prefix, "_", kOutputSize)),
      batch->size()));
  for (int i = 0; i < batch->size(); i++) {
    // If the batch is not full, we only store the first `num_elements` values.
    // The rest of the batch tensor is *uninitialized* and accessing that will
    // raise msan errors.
    if (num_elements < batch_size) {
      TF_RETURN_IF_ERROR(
          writer->WriteTensor(FullName(iterator_prefix, batch_prefix),
                              strings::StrCat(kOutput, "_", i),
                              (*batch)[i].Slice(0, num_elements)));
    } else {
      TF_RETURN_IF_ERROR(
          writer->WriteTensor(FullName(iterator_prefix, batch_prefix),
                              strings::StrCat(kOutput, "_", i), (*batch)[i]));
    }
  }
  return Status::OK();
}

Status ReadStatus(const string& iterator_prefix, const string& prefix,
                  IteratorStateReader* reader, Status* status) {
  int64_t code_int;
  TF_RETURN_IF_ERROR(reader->ReadScalar(
      FullName(iterator_prefix, strings::StrCat(prefix, "_", kCode)),
      &code_int));
  error::Code code = static_cast<error::Code>(code_int);

  if (code != error::Code::OK) {
    tstring error_message;
    TF_RETURN_IF_ERROR(reader->ReadScalar(
        FullName(iterator_prefix, strings::StrCat(prefix, "_", kMessage)),
        &error_message));
    *status = Status(code, error_message);
  } else {
    *status = Status::OK();
  }
  return Status::OK();
}

Status WriteStatus(const string& iterator_prefix, const string& prefix,
                   const Status& status, IteratorStateWriter* writer) {
  TF_RETURN_IF_ERROR(writer->WriteScalar(
      FullName(iterator_prefix, strings::StrCat(prefix, "_", kCode)),
      static_cast<int64_t>(status.code())));
  if (!status.ok()) {
    TF_RETURN_IF_ERROR(writer->WriteScalar(
        FullName(iterator_prefix, strings::StrCat(prefix, "_", kMessage)),
        status.error_message()));
  }
  return Status::OK();
}

Status ProcessBatch(int64_t batch_size, int64_t num_elements,
                    bool drop_remainder, const Status& status,
                    IteratorContext* ctx, std::vector<Tensor>* output,
                    bool* end_of_sequence, std::vector<Tensor>* batch) {
  if (num_elements == 0) {
    if (status.ok() || errors::IsOutOfRange(status)) {
      *end_of_sequence = true;
      return Status::OK();
    } else {
      *end_of_sequence = false;
      return status;
    }
  }
  if (!status.ok() && !errors::IsOutOfRange(status)) {
    *end_of_sequence = false;
    return status;
  }
  if (num_elements < batch_size) {
    if (drop_remainder) {
      *end_of_sequence = true;
      return Status::OK();
    }
    for (size_t i = 0; i < batch->size(); ++i) {
      TensorShape component_shape((*batch)[i].shape());
      component_shape.set_dim(0, num_elements);
      AllocatorAttributes attr;
      attr.set_gpu_compatible(true);
      output->emplace_back(ctx->allocator(attr), (*batch)[i].dtype(),
                           component_shape);
      if (!output->back().IsInitialized()) {
        return errors::ResourceExhausted(
            "Failed to allocate memory for the batch of component ", i);
      }
      TF_RETURN_IF_ERROR(
          CopyPartialBatch(num_elements, (*batch)[i], &output->back()));
    }
  } else {
    *output = std::move(*batch);
  }
  *end_of_sequence = false;
  return Status::OK();
}

Status CopyBatch(CopyBatchParams params,
                 const std::vector<std::vector<Tensor>>& batch_elements,
                 bool parallel_copy,
                 std::function<Status()> allocation_callback,
                 std::vector<Tensor>* out_tensors) {
  const size_t num_tuple_components = batch_elements.at(0).size();
  out_tensors->reserve(num_tuple_components);
  const int64_t num_batch_elements = batch_elements.size();
  for (size_t component_index = 0; component_index < num_tuple_components;
       ++component_index) {
    const Tensor& first_element = batch_elements.at(0)[component_index];
    TensorShape first_element_shape(first_element.shape());
    TensorShape batch_component_shape({num_batch_elements});
    batch_component_shape.AppendShape(first_element_shape);
    out_tensors->emplace_back(params.allocator, first_element.dtype(),
                              batch_component_shape);
    if (!out_tensors->back().IsInitialized()) {
      return errors::ResourceExhausted(
          "Failed to allocate memory for the batch of component ",
          component_index);
    }
  }
  if (allocation_callback) {
    TF_RETURN_IF_ERROR(allocation_callback());
  }
  for (size_t component_index = 0; component_index < num_tuple_components;
       ++component_index) {
    Tensor& batch_component = out_tensors->at(component_index);
    const Tensor& first_element = batch_elements.at(0)[component_index];
    TensorShape first_element_shape(first_element.shape());
    // Build the output tuple component by copying one slice from each input
    // element in the batch.
    auto copy_element_fn = [component_index, &batch_elements, &batch_component,
                            &first_element_shape](int index) {
      if (batch_elements.at(index)[component_index].shape() !=
          first_element_shape) {
        return errors::InvalidArgument(
            "Cannot batch tensors with different shapes in component ",
            component_index, ". First element had shape ",
            first_element_shape.DebugString(), " and element ", index,
            " had shape ",
            batch_elements.at(index)[component_index].shape().DebugString(),
            ".");
      }
      return batch_util::CopyElementToSlice(
          std::move(batch_elements.at(index)[component_index]),
          &batch_component, index);
    };
    if (parallel_copy && first_element.AllocatedBytes() > (1 << 15)) {
      Status status;
      mutex status_mu;
      BlockingCounter counter(num_batch_elements);
      const auto num_threads = params.runner_threadpool_size;
      const auto slice_size = num_batch_elements / num_threads;
      int64_t offset = 0;
      for (size_t i = 0; i < num_threads; ++i) {
        int64_t length = slice_size;
        // When the number of threads does not divide the number of elements
        // evenly, the size of some slices is incremented to guarantee their
        // sizes add up to the total number of elements.
        if (i < num_batch_elements % num_threads) ++length;
        (*params.runner)([offset, length, &status, &status_mu, &counter,
                          &copy_element_fn]() {
          for (size_t j = offset; j < offset + length; ++j) {
            {
              Status s = copy_element_fn(j);
              mutex_lock l(status_mu);
              status.Update(s);
            }
            counter.DecrementCount();
          }
        });
        offset += length;
      }
      counter.Wait();
      TF_RETURN_IF_ERROR(status);
    } else {
      for (size_t i = 0; i < num_batch_elements; ++i) {
        TF_RETURN_IF_ERROR(copy_element_fn(i));
      }
    }
  }
  return Status::OK();
}

absl::flat_hash_set<tstring> CreateGraphRewriteConfigs(const Options& options) {
  absl::flat_hash_set<tstring> configs;
  const auto& autotune_options = options.autotune_options();
  std::vector<tstring> autotune_only_optimizations = {
      kAutotuneBufferSizesOpt,
      kBatchParallelizationOpt,
      kDisablePrefetchLegacyAutotuneOpt,
      kEnableGradientDescentOpt,
      kMapParallelizationOpt,
      kInjectPrefetchOpt,
      kInjectPrefetchEligibleOpt};

  if (autotune_options.optional_enabled_case() == AutotuneOptions::kEnabled &&
      !autotune_options.enabled()) {
    for (const auto& optimization : autotune_only_optimizations) {
      configs.insert(
          absl::StrCat(optimization.data(), ":", kAutotuneOpt, ":false"));
    }
  } else {
    for (const auto& optimization : autotune_only_optimizations) {
      configs.insert(
          absl::StrCat(optimization.data(), ":", kAutotuneOpt, ":true"));
    }
  }
  if (options.slack()) {
    int num_devices = 1;
    if (options.distribute_options().optional_num_devices_case() ==
        DistributeOptions::kNumDevices) {
      num_devices = options.distribute_options().num_devices();
    }
    configs.insert(
        absl::StrCat(kSlackOpt, ":", kSlackPeriodOpt, ":", num_devices));
  }
  return configs;
}

bool ShouldConfigureMaxIntraOpParallelism(const Options& options) {
  return options.threading_options().optional_max_intra_op_parallelism_case() ==
         ThreadingOptions::kMaxIntraOpParallelism;
}

bool ShouldUsePrivateThreadPool(const Options& options) {
  return options.threading_options().optional_private_threadpool_size_case() ==
         ThreadingOptions::kPrivateThreadpoolSize;
}

bool ShouldUseAutotuning(const Options& options) {
  return options.autotune_options().optional_enabled_case() !=
             AutotuneOptions::kEnabled ||
         options.autotune_options().enabled();
}

bool ShouldApplyOptimizations(
    const Options& options,
    const absl::flat_hash_set<tstring>& optimizations_enabled,
    const absl::flat_hash_set<tstring>& optimizations_default) {
  return (options.optimization_options()
                  .optional_apply_default_optimizations_case() !=
              OptimizationOptions::kApplyDefaultOptimizations ||
          options.optimization_options().apply_default_optimizations() ||
          !optimizations_enabled.empty() || !optimizations_default.empty());
}

int64 GetAutotuneDefaultParallelism(IteratorContext* ctx) {
  if (GetExperiments().contains("initial_parallelism_value")) {
    return std::min(kAutotuneDefaultParallelism, ctx->runner_threadpool_size());
  }
  return ctx->runner_threadpool_size();
}

// static
void DatasetExperimentRegistry::Register(const string& experiment,
                                         int64_t rollout_pct) {
  mutex_lock l(*get_dataset_experiment_registry_lock());
  get_dataset_experiments()->insert(std::make_pair(experiment, rollout_pct));
}

// static
absl::flat_hash_map<string, int64_t> DatasetExperimentRegistry::Experiments() {
  mutex_lock l(*get_dataset_experiment_registry_lock());
  return *get_dataset_experiments();
}

namespace {

REGISTER_DATASET_EXPERIMENT("initial_parallelism_value", 0);
REGISTER_DATASET_EXPERIMENT("enable_bufferedio_v2", 50);
REGISTER_DATASET_EXPERIMENT("inject_prefetch", 50);
REGISTER_DATASET_EXPERIMENT("max_parallelism", 100);
REGISTER_DATASET_EXPERIMENT("max_parallelism_v2", 0);
REGISTER_DATASET_EXPERIMENT("min_outer_interleave_parallelism", 0);
}  // namespace
}  // namespace data
}  // namespace tensorflow
