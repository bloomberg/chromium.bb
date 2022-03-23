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
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/utils/tf_op_utils.h"

namespace tensorflow {
namespace profiler {

const absl::string_view kHostThreadsPlaneName = "/host:CPU";
const absl::string_view kGpuPlanePrefix = "/device:GPU:";
const absl::string_view kTpuPlanePrefix = "/device:TPU:";
// TODO(b/195582092): change it to /device:custom once all literals are
// migrated.
const absl::string_view kCustomPlanePrefix = "/device:CUSTOM:";

const absl::string_view kTpuRuntimePlaneName = "/host:TPU-runtime";
const absl::string_view kCuptiDriverApiPlaneName = "/host:CUPTI";
const absl::string_view kRoctracerApiPlaneName = "/host:ROCTRACER";
const absl::string_view kMetadataPlaneName = "/host:metadata";
const absl::string_view kTFStreamzPlaneName = "/host:tfstreamz";
const absl::string_view kPythonTracerPlaneName = "/host:python-tracer";

const absl::string_view kStepLineName = "Steps";
const absl::string_view kTensorFlowNameScopeLineName = "TensorFlow Name Scope";
const absl::string_view kTensorFlowOpLineName = "TensorFlow Ops";
const absl::string_view kXlaModuleLineName = "XLA Modules";
const absl::string_view kXlaOpLineName = "XLA Ops";
const absl::string_view kKernelLaunchLineName = "Launch Stats";
const absl::string_view kSourceLineName = "Source code";

const absl::string_view kDeviceVendorNvidia = "Nvidia";
const absl::string_view kDeviceVendorAMD = "AMD";

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
      {"EagerExecute", kEagerKernelExecute},
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
      {"Iterator", kIterator},
      {"Iterator::Prefetch::Generator", kDeviceInputPipelineSecondIterator},
      {"PrefetchProduce", kPrefetchProduce},
      {"PrefetchConsume", kPrefetchConsume},
      {"ParallelInterleaveProduce", kParallelInterleaveProduce},
      {"ParallelInterleaveConsume", kParallelInterleaveConsume},
      {"ParallelInterleaveInitializeInput",
       kParallelInterleaveInitializedInput},
      {"ParallelMapProduce", kParallelMapProduce},
      {"ParallelMapConsume", kParallelMapConsume},
      {"MapAndBatchProduce", kMapAndBatchProduce},
      {"MapAndBatchConsume", kMapAndBatchConsume},
      {"ParseExampleProduce", kParseExampleProduce},
      {"ParseExampleConsume", kParseExampleConsume},
      {"ParallelBatchProduce", kParallelBatchProduce},
      {"ParallelBatchConsume", kParallelBatchConsume},
      // Batching related.
      {"BatchingSessionRun", kBatchingSessionRun},
      {"ProcessBatch", kProcessBatch},
      {"ConcatInputTensors", kConcatInputTensors},
      {"MergeInputTensors", kMergeInputTensors},
      {"ScheduleWithoutSplit", kScheduleWithoutSplit},
      {"ScheduleWithSplit", kScheduleWithSplit},
      {"ScheduleWithEagerSplit", kScheduleWithEagerSplit},
      {"ASBSQueue::Schedule", kASBSQueueSchedule},
      // TFRT related.
      {"TfrtModelRun", kTfrtModelRun},
      // JAX related.
      {"LocalExecutable::ExecuteOnLocalDevices", kExecuteOnLocalDevices},
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
      {"queue_id", kQueueId},
      {"request_id", kRequestId},
      {"run_id", kRunId},
      {"replica_id", kReplicaId},
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
      {"layout", kTensorLayout},
      {"kpi_name", kKpiName},
      {"kpi_value", kKpiValue},
      {"element_id", kElementId},
      {"parent_id", kParentId},
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
      {"MemFree_details", kMemFreeDetails},
      {"Memset_details", kMemsetDetails},
      {"MemoryResidency_details", kMemoryResidencyDetails},
      {"kernel_details", kKernelDetails},
      {"nvtx_range", kNVTXRange},
      {"stream", kStream},
      // Stats added when processing traces.
      {"group_id", kGroupId},
      {"flow", kFlow},
      {"step_name", kStepName},
      {"tf_op", kTfOp},
      {"hlo_op", kHloOp},
      {"hlo_category", kHloCategory},
      {"hlo_module", kHloModule},
      {"program_id", kProgramId},
      {"equation", kEquation},
      {"is_eager", kIsEager},
      {"is_func", kIsFunc},
      {"tf_function_call", kTfFunctionCall},
      {"tracing_count", kTfFunctionTracingCount},
      {"flops", kFlops},
      {"bytes_accessed", kBytesAccessed},
      {"selected_group_ids", kSelectedGroupIds},
      {"source", kSourceInfo},
      {"model_name", kModelName},
      {"model_version", kModelVersion},
      {"bytes_transferred", kBytesTransferred},
      {"queue", kDmaQueue},
      // Performance counter related.
      {"Raw Value", kRawValue},
      {"Scaled Value", kScaledValue},
      {"Thread Id", kThreadId},
      // XLA metadata map related.
      {"Hlo Proto", kHloProto},
      // Device capability related.
      {"clock_rate", kDevCapClockRateKHz},
      {"core_count", kDevCapCoreCount},
      {"memory_bandwidth", kDevCapMemoryBandwidth},
      {"memory_size", kDevCapMemorySize},
      {"compute_cap_major", kDevCapComputeCapMajor},
      {"compute_cap_minor", kDevCapComputeCapMinor},
      {"device_vendor", kDevVendor},
      // Batching related.
      {"batch_size_after_padding", kBatchSizeAfterPadding},
      {"padding_amount", kPaddingAmount},
      {"batching_input_task_size", kBatchingInputTaskSize},
      // GPU related metrics.
      {"theoretical_occupancy_pct", kTheoreticalOccupancyPct},
      {"occupancy_min_grid_size", kOccupancyMinGridSize},
      {"occupancy_suggested_block_size", kOccupancySuggestedBlockSize},
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

absl::optional<int64_t> FindHostEventType(absl::string_view event_name) {
  if (auto event_type = gtl::FindOrNull(GetHostEventTypeMap(), event_name)) {
    return *event_type;
  }
  return absl::nullopt;
}

absl::optional<int64_t> FindTfOpEventType(absl::string_view event_name) {
  // TF op names.
  Category category = ParseTfOpFullname(event_name).category;
  switch (category) {
    case Category::kTensorFlow:
      return HostEventType::kTfOpRun;
    case Category::kTfData:
      return HostEventType::kIterator;
    default:
      return absl::nullopt;
  }
}

absl::string_view GetStatTypeStr(StatType stat_type) {
  return GetStatTypeStrMap().at(stat_type);
}

absl::optional<int64_t> FindStatType(absl::string_view stat_name) {
  if (auto stat_type = gtl::FindOrNull(GetStatTypeMap(), stat_name)) {
    return *stat_type;
  }
  return absl::nullopt;
}

bool IsInternalEvent(absl::optional<int64_t> event_type) {
  // TODO(b/162102421): Introduce a prefix for internal event names.
  if (!event_type.has_value()) return false;
  switch (*event_type) {
    case HostEventType::kMemoryAllocation:
    case HostEventType::kMemoryDeallocation:
    case HostEventType::kPrefetchProduce:
    case HostEventType::kPrefetchConsume:
    case HostEventType::kParallelInterleaveProduce:
    case HostEventType::kParallelInterleaveConsume:
    case HostEventType::kParallelInterleaveInitializedInput:
    case HostEventType::kParallelMapProduce:
    case HostEventType::kParallelMapConsume:
    case HostEventType::kMapAndBatchProduce:
    case HostEventType::kMapAndBatchConsume:
    case HostEventType::kParseExampleProduce:
    case HostEventType::kParseExampleConsume:
      return true;
    default:
      return false;
  }
}

bool IsInternalStat(absl::optional<int64_t> stat_type) {
  // TODO(b/162102421): Introduce a prefix for internal stat names.
  if (!stat_type.has_value()) return false;
  switch (*stat_type) {
    case StatType::kKernelDetails:
    case StatType::kProducerType:
    case StatType::kProducerId:
    case StatType::kConsumerType:
    case StatType::kConsumerId:
    case StatType::kIsRoot:
    case StatType::kIsAsync:
    case StatType::kFlops:
    case StatType::kBytesAccessed:
      return true;
    default:
      return false;
  }
}

}  // namespace profiler
}  // namespace tensorflow
