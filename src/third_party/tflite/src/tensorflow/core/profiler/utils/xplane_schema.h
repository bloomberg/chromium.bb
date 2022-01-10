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

#ifndef TENSORFLOW_CORE_PROFILER_UTILS_XPLANE_SCHEMA_H_
#define TENSORFLOW_CORE_PROFILER_UTILS_XPLANE_SCHEMA_H_

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace profiler {

// Name of XPlane that contains TraceMe events.
TF_CONST_INIT extern const absl::string_view kHostThreadsPlaneName;
// Name prefix of XPlane that contains GPU events.
TF_CONST_INIT extern const absl::string_view kGpuPlanePrefix;
// Name prefix of XPlane that contains TPU events.
TF_CONST_INIT extern const absl::string_view kTpuPlanePrefix;
// Name prefix of XPlane that contains custom device events.
TF_CONST_INIT extern const absl::string_view kCustomPlanePrefix;
// Name prefix of XPlane that contains TPU runtime events.
TF_CONST_INIT extern const absl::string_view kTpuRuntimePlaneName;
// Name of XPlane that contains CUPTI driver API generated events.
TF_CONST_INIT extern const absl::string_view kCuptiDriverApiPlaneName;
// Name of XPlane that contains Roctracer API generated events.
TF_CONST_INIT extern const absl::string_view kRoctracerApiPlaneName;
// Name of XPlane that contains profile metadata such as XLA debug info.
TF_CONST_INIT extern const absl::string_view kMetadataPlaneName;
// Name of XPlane that contains kpi related metrics.
TF_CONST_INIT extern const absl::string_view kTFStreamzPlaneName;
// Name of XPlane that contains events from python tracer.
TF_CONST_INIT extern const absl::string_view kPythonTracerPlaneName;

// Names of XLines that contain ML-level events.
TF_CONST_INIT extern const absl::string_view kStepLineName;
TF_CONST_INIT extern const absl::string_view kTensorFlowNameScopeLineName;
TF_CONST_INIT extern const absl::string_view kTensorFlowOpLineName;
TF_CONST_INIT extern const absl::string_view kXlaModuleLineName;
TF_CONST_INIT extern const absl::string_view kXlaOpLineName;
TF_CONST_INIT extern const absl::string_view kKernelLaunchLineName;
TF_CONST_INIT extern const absl::string_view kSourceLineName;

// GPU device vendors.
TF_CONST_INIT extern const absl::string_view kDeviceVendorNvidia;
TF_CONST_INIT extern const absl::string_view kDeviceVendorAMD;

// Interesting event types (i.e., TraceMe names).
enum HostEventType {
  kFirstHostEventType = 0,
  kUnknownHostEventType = kFirstHostEventType,
  kTraceContext,
  kSessionRun,
  kFunctionRun,
  kRunGraph,
  kRunGraphDone,
  kTfOpRun,
  kEagerKernelExecute,
  kExecutorStateProcess,
  kExecutorDoneCallback,
  kMemoryAllocation,
  kMemoryDeallocation,
  // Performance counter related.
  kRemotePerf,
  // tf.data captured function events.
  kTfDataCapturedFunctionRun,
  kTfDataCapturedFunctionRunWithBorrowedArgs,
  kTfDataCapturedFunctionRunInstantiated,
  kTfDataCapturedFunctionRunAsync,
  // Functional ops.
  kCallOp,
  kParallelForOp,
  kForeverOp,
  kNumericalGradientOpEvalRight,
  kNumericalGradientOpEvalLeft,
  kSymbolicGradientOp,
  kRemoteCallOp,
  kIfOp,
  kCaseOp,
  kWhileOpEvalCond,
  kWhileOpStartBody,
  kForOp,
  kPartitionedCallOp,
  // tf.data related.
  kIteratorGetNextOp,
  kIteratorGetNextAsOptionalOp,
  kIterator,
  kDeviceInputPipelineSecondIterator,
  kPrefetchProduce,
  kPrefetchConsume,
  kParallelInterleaveProduce,
  kParallelInterleaveConsume,
  kParallelInterleaveInitializedInput,
  kParallelMapProduce,
  kParallelMapConsume,
  kMapAndBatchProduce,
  kMapAndBatchConsume,
  kParseExampleProduce,
  kParseExampleConsume,
  kParallelBatchProduce,
  kParallelBatchConsume,
  // Batching related.
  kBatchingSessionRun,
  kProcessBatch,
  kConcatInputTensors,
  kMergeInputTensors,
  kScheduleWithoutSplit,
  kScheduleWithSplit,
  kScheduleWithEagerSplit,
  kASBSQueueSchedule,
  // TFRT related.
  kTfrtModelRun,
  // JAX related.
  kExecuteOnLocalDevices,
  // GPU related.
  kKernelLaunch,
  kKernelExecute,
  kLastHostEventType = kKernelExecute,
};

enum StatType {
  kFirstStatType = 0,
  kUnknownStatType = kFirstStatType,
  // TraceMe arguments.
  kStepId,
  kParentStepId,
  kFunctionStepId,
  kDeviceOrdinal,
  kChipOrdinal,
  kNodeOrdinal,
  kModelId,
  kQueueId,
  kQueueAddr,
  kRequestId,
  kRunId,
  kReplicaId,
  kGraphType,
  kStepNum,
  kIterNum,
  kIndexOnHost,
  kAllocatorName,
  kBytesReserved,
  kBytesAllocated,
  kBytesAvailable,
  kFragmentation,
  kPeakBytesInUse,
  kRequestedBytes,
  kAllocationBytes,
  kAddress,
  kRegionType,
  kDataType,
  kTensorShapes,
  kTensorLayout,
  kKpiName,
  kKpiValue,
  kElementId,
  kParentId,
  // XPlane semantics related.
  kProducerType,
  kConsumerType,
  kProducerId,
  kConsumerId,
  kIsRoot,
  kIsAsync,
  // Device trace arguments.
  kDeviceId,
  kContextId,
  kCorrelationId,
  // TODO(b/176137043): These "details" should differentiate between activity
  // and API event sources.
  kMemcpyDetails,
  kMemallocDetails,
  kMemFreeDetails,
  kMemsetDetails,
  kMemoryResidencyDetails,
  kNVTXRange,
  kKernelDetails,
  kStream,
  // Stats added when processing traces.
  kGroupId,
  kFlow,
  kStepName,
  kLevel0,
  kTfOp,
  kHloOp,
  kHloCategory,
  kHloModule,
  kProgramId,
  kEquation,
  kIsEager,
  kTfFunctionCall,
  kTfFunctionTracingCount,
  kFlops,
  kBytesAccessed,
  kSelectedGroupIds,
  kSourceInfo,
  kModelName,
  kModelVersion,
  kBytesTransferred,
  kDmaQueue,
  // Performance counter related.
  kRawValue,
  kScaledValue,
  kThreadId,
  // XLA metadata map related.
  kHloProto,
  // Device capability related.
  kDevCapClockRateKHz,
  kDevCapCoreCount,
  kDevCapMemoryBandwidth,
  kDevCapMemorySize,
  kDevCapComputeCapMajor,
  kDevCapComputeCapMinor,
  kDevVendor,
  // Batching related.
  kBatchSizeAfterPadding,
  kPaddingAmount,
  kBatchingInputTaskSize,
  // GPU occupancy metrics
  kTheoreticalOccupancyPct,
  kOccupancyMinGridSize,
  kOccupancySuggestedBlockSize,
  kLastStatType = kOccupancySuggestedBlockSize,
};

inline std::string GpuPlaneName(int32_t device_ordinal) {
  return absl::StrCat(kGpuPlanePrefix, device_ordinal);
}

absl::string_view GetHostEventTypeStr(HostEventType event_type);

bool IsHostEventType(HostEventType event_type, absl::string_view event_name);

inline bool IsHostEventType(HostEventType event_type,
                            absl::string_view event_name) {
  return GetHostEventTypeStr(event_type) == event_name;
}

absl::optional<int64_t> FindHostEventType(absl::string_view event_name);

absl::optional<int64_t> FindTfOpEventType(absl::string_view event_name);

absl::string_view GetStatTypeStr(StatType stat_type);

bool IsStatType(StatType stat_type, absl::string_view stat_name);

inline bool IsStatType(StatType stat_type, absl::string_view stat_name) {
  return GetStatTypeStr(stat_type) == stat_name;
}

absl::optional<int64_t> FindStatType(absl::string_view stat_name);

// Returns true if the given event shouldn't be shown in the trace viewer.
bool IsInternalEvent(absl::optional<int64_t> event_type);

// Returns true if the given stat shouldn't be shown in the trace viewer.
bool IsInternalStat(absl::optional<int64_t> stat_type);

// Support for flow events:
// This class enables encoding/decoding the flow id and direction, stored as
// XStat value.
class XFlow {
 public:
  enum FlowDirection {
    kFlowUnspecified = 0x0,
    kFlowIn = 0x1,
    kFlowOut = 0x2,
    kFlowInOut = 0x3,
  };

  XFlow(uint64 flow_id, FlowDirection direction)
      : encoded_((flow_id << 2) | (direction & 0x3)) {
    DCHECK_NE(Direction(), kFlowUnspecified);
  }

  // Encoding
  uint64 ToStatValue() const { return encoded_; }

  // Decoding
  static XFlow FromStatValue(uint64 encoded) { return XFlow(encoded); }

  uint64 Id() const { return (encoded_ >> 2); }
  FlowDirection Direction() const { return FlowDirection(encoded_ & 0x3); }

 private:
  explicit XFlow(uint64 encoded) : encoded_(encoded) {}

  uint64 encoded_;
};

}  // namespace profiler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PROFILER_UTILS_XPLANE_SCHEMA_H_
