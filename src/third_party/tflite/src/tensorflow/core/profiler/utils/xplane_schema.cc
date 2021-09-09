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

#include "tensorflow/core/profiler/utils/xplane_schema.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace profiler {

const absl::string_view kHostThreadsPlaneName = "/host:CPU";
const absl::string_view kGpuPlanePrefix = "/device:GPU:";
const absl::string_view kCuptiDriverApiPlaneName = "/host:CUPTI";
const absl::string_view kMetadataPlaneName = "/host:metadata";
const absl::string_view kTFStreamzPlaneName = "/host:tfstreamz";

const absl::string_view kStepLineName = "Steps";
const absl::string_view kTensorFlowNameScopeLineName = "TensorFlow Name Scope";
const absl::string_view kTensorFlowOpLineName = "TensorFlow Ops";
const absl::string_view kXlaModuleLineName = "XLA Modules";
const absl::string_view kXlaOpLineName = "XLA Ops";
const absl::string_view kKernelLaunchLineName = "Launch Stats";

namespace {

constexpr int kNumHostEventTypes =
    HostEventType::kLastHostEventType - HostEventType::kFirstHostEventType + 1;

constexpr int kNumStatTypes =
    StatType::kLastStatType - StatType::kFirstStatType + 1;

using HostEventTypeMap = absl::flat_hash_map<absl::string_view, HostEventType>;
using HostEventTypeStrMap =
    absl::flat_hash_map<HostEventType, absl::string_view>;
using StatTypeMap = absl::flat_hash_map<absl::string_view, StatType>;
using StatTypeStrMap = absl::flat_hash_map<StatType, absl::string_view>;

const HostEventTypeMap& GetHostEventTypeMap() {
  static auto* host_event_type_map = new HostEventTypeMap({
      {"UnknownHostEventType", kUnknownHostEventType},
      {"TraceContext", kTraceContext},
      {"SessionRun", kSessionRun},
      {"FunctionRun", kFunctionRun},
      {"RunGraph", kRunGraph},
      {"RunGraphDone", kRunGraphDone},
      {"TfOpRun", kTfOpRun},
      {"EagerKernelExecute", kEagerKernelExecute},
      {"ExecutorState::Process", kExecutorStateProcess},
      {"ExecutorDoneCallback", kExecutorDoneCallback},
      {"MemoryAllocation", kMemoryAllocation},
      {"MemoryDeallocation", kMemoryDeallocation},
      // Performance counter related.
      {"RemotePerfCounter", kRemotePerf},
      // tf data captured function events.
      {"InstantiatedCapturedFunction::Run", kTfDataCapturedFunctionRun},
      {"InstantiatedCapturedFunction::RunWithBorrowedArgs",
       kTfDataCapturedFunctionRunWithBorrowedArgs},
      {"InstantiatedCapturedFunction::RunInstantiated",
       kTfDataCapturedFunctionRunInstantiated},
      {"InstantiatedCapturedFunction::RunAsync",
       kTfDataCapturedFunctionRunAsync},
      // Functional ops.
      {"CallOp", kCallOp},
      {"ParallelForOp", kParallelForOp},
      {"ForeverOp", kForeverOp},
      {"NumericalGradientOp-EvalRight", kNumericalGradientOpEvalRight},
      {"NumericalGradientOp-EvalLeft", kNumericalGradientOpEvalLeft},
      {"SymbolicGradientOp", kSymbolicGradientOp},
      {"RemoteCallOp", kRemoteCallOp},
      {"IfOp", kIfOp},
      {"CaseOp", kCaseOp},
      {"WhileOp-EvalCond", kWhileOpEvalCond},
      {"WhileOp-StartBody", kWhileOpStartBody},
      {"ForOp", kForOp},
      {"PartitionedCallOp", kPartitionedCallOp},
      // tf.data related.
      {"IteratorGetNextOp::DoCompute", kIteratorGetNextOp},
      {"IteratorGetNextAsOptionalOp::DoCompute", kIteratorGetNextAsOptionalOp},
      // GPU related.
      {"KernelLaunch", kKernelLaunch},
      {"KernelExecute", kKernelExecute},
  });
  DCHECK_EQ(host_event_type_map->size(), kNumHostEventTypes);
  return *host_event_type_map;
}

const StatTypeMap& GetStatTypeMap() {
  static auto* stat_type_map = new StatTypeMap({
      {"UnknownStatType", kUnknownStatType},
      // TraceMe arguments.
      {"id", kStepId},
      {"parent_step_id", kParentStepId},
      {"function_step_id", kFunctionStepId},
      {"device_ordinal", kDeviceOrdinal},
      {"chip_ordinal", kChipOrdinal},
      {"node_ordinal", kNodeOrdinal},
      {"model_id", kModelId},
      {"queue_addr", kQueueAddr},
      {"request_id", kRequestId},
      {"run_id", kRunId},
      {"graph_type", kGraphType},
      {"step_num", kStepNum},
      {"iter_num", kIterNum},
      {"index_on_host", kIndexOnHost},
      {"allocator_name", kAllocatorName},
      {"bytes_reserved", kBytesReserved},
      {"bytes_allocated", kBytesAllocated},
      {"bytes_available", kBytesAvailable},
      {"fragmentation", kFragmentation},
      {"peak_bytes_in_use", kPeakBytesInUse},
      {"requested_bytes", kRequestedBytes},
      {"allocation_bytes", kAllocationBytes},
      {"addr", kAddress},
      {"region_type", kRegionType},
      {"data_type", kDataType},
      {"shape", kTensorShapes},
      {"kpi_name", kKpiName},
      {"kpi_value", kKpiValue},
      // XPlane semantics related.
      {"_pt", kProducerType},
      {"_ct", kConsumerType},
      {"_p", kProducerId},
      {"_c", kConsumerId},
      {"_r", kIsRoot},
      {"_a", kIsAsync},
      // Device trace arguments.
      {"device_id", kDeviceId},
      {"context_id", kContextId},
      {"correlation_id", kCorrelationId},
      {"memcpy_details", kMemcpyDetails},
      {"memalloc_details", kMemallocDetails},
      {"kernel_details", kKernelDetails},
      {"annotation", kKernelAnnotation},
      {"stream", kStream},
      // Stats added when processing traces.
      {"group_id", kGroupId},
      {"flow", kFlow},
      {"step_name", kStepName},
      {"level 0", kLevel0},
      {"tf_op", kTfOp},
      {"hlo_op", kHloOp},
      {"hlo_module", kHloModule},
      {"equation", kEquation},
      {"is_eager", kIsEager},
      {"tf_function_call", kTfFunctionCall},
      {"tracing_count", kTfFunctionTracingCount},
      {"flops", kFlops},
      {"bytes_accessed", kBytesAccessed},
      // Performance counter related.
      {"Raw Value", kRawValue},
      {"Scaled Value", kScaledValue},
      {"Thread Id", kThreadId},
      // XLA metadata map related.
      {"SELF_DURATION_PS", kSelfDurationPs},
      {"MIN_DURATION_PS", kMinDurationPs},
      {"Hlo Proto", kHloProto},
      // Device capability related.
      {"clock_rate", kDevCapClockRateKHz},
      {"core_count", kDevCapCoreCount},
      {"memory_bandwidth", kDevCapMemoryBandwidth},
      {"memory_size", kDevCapMemorySize},
      {"compute_cap_major", kDevCapComputeCapMajor},
      {"compute_cap_minor", kDevCapComputeCapMinor},
  });
  DCHECK_EQ(stat_type_map->size(), kNumStatTypes);
  return *stat_type_map;
}

const HostEventTypeStrMap& GetHostEventTypeStrMap() {
  static auto* host_event_type_str_map = new HostEventTypeStrMap(
      gtl::ReverseMap<HostEventTypeStrMap>(GetHostEventTypeMap()));
  return *host_event_type_str_map;
}

const StatTypeStrMap& GetStatTypeStrMap() {
  static auto* stat_type_str_map =
      new StatTypeStrMap(gtl::ReverseMap<StatTypeStrMap>(GetStatTypeMap()));
  return *stat_type_str_map;
}

}  // namespace

absl::string_view GetHostEventTypeStr(HostEventType event_type) {
  return GetHostEventTypeStrMap().at(event_type);
}

absl::optional<int64> FindHostEventType(absl::string_view event_name) {
  if (auto event_type = gtl::FindOrNull(GetHostEventTypeMap(), event_name)) {
    return *event_type;
  }
  return absl::nullopt;
}

absl::string_view GetStatTypeStr(StatType stat_type) {
  return GetStatTypeStrMap().at(stat_type);
}

absl::optional<int64> FindStatType(absl::string_view stat_name) {
  if (auto stat_type = gtl::FindOrNull(GetStatTypeMap(), stat_name)) {
    return *stat_type;
  }
  return absl::nullopt;
}

bool IsInternalStat(absl::optional<int64> stat_type) {
  static const auto* const kInternalStats = new absl::flat_hash_set<int64>{
      StatType::kKernelDetails, StatType::kLevel0,
      StatType::kProducerType,  StatType::kProducerId,
      StatType::kConsumerType,  StatType::kConsumerId,
      StatType::kIsRoot,        StatType::kIsAsync,
      StatType::kFlops,         StatType::kBytesAccessed};
  return stat_type.has_value() && kInternalStats->contains(*stat_type);
}

}  // namespace profiler
}  // namespace tensorflow
