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

#include "tensorflow/core/profiler/convert/xplane_to_step_events.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/protobuf/xplane.pb.h"
#include "tensorflow/core/profiler/utils/event_span.h"
#include "tensorflow/core/profiler/utils/tf_xplane_visitor.h"
#include "tensorflow/core/profiler/utils/timespan.h"
#include "tensorflow/core/profiler/utils/trace_utils.h"
#include "tensorflow/core/profiler/utils/xplane_schema.h"
#include "tensorflow/core/profiler/utils/xplane_visitor.h"

namespace tensorflow {
namespace profiler {
namespace {

inline bool IsExplicitHostStepMarker(absl::string_view event_name) {
  return (absl::StartsWith(event_name, "train") ||
          absl::StartsWith(event_name, "test") ||
          absl::StartsWith(event_name, "TraceContext")) &&
         !absl::StrContains(event_name, "/");
}

// Returns true if the given event_name should be considered as real computation
// on CPU.
inline bool IsRealCpuCompute(absl::string_view event_name) {
  bool not_real = absl::StartsWith(event_name, "EagerExecute") ||
                  absl::StartsWith(event_name, "EagerLocalExecute") ||
                  absl::StartsWith(event_name, "EagerKernelExecute") ||
                  absl::StartsWith(event_name, "FunctionRun") ||
                  IsExplicitHostStepMarker(event_name);
  return !not_real;
}

}  // namespace

StepEvents ConvertHostThreadsXLineToStepEvents(
    const XLineVisitor& line, bool use_device_step_events,
    const StepEvents& device_step_events) {
  StepEvents result;
  line.ForEachEvent([&](const XEventVisitor& event) {
    int64 correlation_id = -1;
    int64 group_id = -1;
    absl::string_view step_name;
    event.ForEachStat([&](const XStatVisitor& stat) {
      if (!stat.Type().has_value()) return;
      switch (stat.Type().value()) {
        case StatType::kCorrelationId:
          correlation_id = stat.IntValue();
          break;
        case StatType::kGroupId:
          group_id = stat.IntValue();
          break;
        case StatType::kStepName:
          step_name = stat.StrOrRefValue();
          break;
      }
    });
    if (group_id < 0) return;
    // Don't add CPU events when (1) it includes device step events and (2) it
    // doesn't have a device and that the group_id (i.e. step number) already
    // appears on the device. This will filter out all cpu events that do not
    // correspond to any steps executed on the device.
    if (use_device_step_events &&
        device_step_events.find(group_id) == device_step_events.end())
      return;
    Timespan timespan = Timespan(event.TimestampPs(), event.DurationPs());
    if (IsExplicitHostStepMarker(event.Name())) {
      result[group_id].AddMarker(StepMarker(
          StepMarkerType::kExplicitHostStepMarker, event.Name(), timespan));
    } else if (!step_name.empty()) {
      // Grouping adds a step_name stat to implicit host step markers.
      result[group_id].AddMarker(StepMarker(
          StepMarkerType::kImplicitHostStepMarker, event.Name(), timespan));
    } else if (IsRealCpuCompute(event.Name())) {
      EventTypeSpan event_type_span(
          ClassifyCpuEvent(event.Name(), correlation_id,
                           use_device_step_events),
          timespan);
      result[group_id].AddEvent(event_type_span);
    }
  });
  return result;
}

StepEvents ConvertHostThreadsXPlaneToStepEvents(
    const XPlane& host_trace, bool use_device_step_events,
    const StepEvents& device_step_events) {
  StepEvents result;
  XPlaneVisitor plane = CreateTfXPlaneVisitor(&host_trace);
  plane.ForEachLine([&](const XLineVisitor& line) {
    CombineStepEvents(ConvertHostThreadsXLineToStepEvents(
                          line, use_device_step_events, device_step_events),
                      &result);
  });
  return result;
}

StepEvents ConvertDeviceStepInfoToStepMarkers(const XLineVisitor& line) {
  StepEvents result;
  line.ForEachEvent([&](const XEventVisitor& event) {
    if (absl::optional<XStatVisitor> stat = event.GetStat(StatType::kGroupId)) {
      result[stat->IntValue()].AddMarker(
          StepMarker(StepMarkerType::kDeviceStepMarker, event.Name(),
                     Timespan(event.TimestampPs(), event.DurationPs())));
    }
  });
  return result;
}

StepEvents ConvertDeviceTraceXLineToStepEvents(const XLineVisitor& line) {
  StepEvents result;
  line.ForEachEvent([&](const XEventVisitor& event) {
    int64 correlation_id = -1;
    int64 group_id = -1;
    absl::string_view tensor_shapes;
    event.ForEachStat([&](const XStatVisitor& stat) {
      if (!stat.Type().has_value()) return;
      switch (stat.Type().value()) {
        case StatType::kCorrelationId:
          correlation_id = stat.IntValue();
          break;
        case StatType::kGroupId:
          group_id = stat.IntValue();
          break;
        case StatType::kTensorShapes:
          tensor_shapes = stat.StrOrRefValue();
          break;
      }
    });

    if (correlation_id >= 0 && group_id >= 0) {
      EventTypeSpan event_type_span(
          ClassifyGpuEvent(event.Name(), tensor_shapes),
          Timespan(event.TimestampPs(), event.DurationPs()));
      result[group_id].AddEvent(event_type_span);
    }
  });
  return result;
}

StepEvents ConvertDeviceTraceXPlaneToStepEvents(const XPlane& device_trace) {
  StepEvents result;
  XPlaneVisitor plane = CreateTfXPlaneVisitor(&device_trace);
  plane.ForEachLine([&](const XLineVisitor& line) {
    int64 line_id = line.Id();
    if (line_id == kThreadIdStepInfo) {
      CombineStepEvents(ConvertDeviceStepInfoToStepMarkers(line), &result);
    } else if (IsDerivedThreadId(line_id)) {
      return;
    } else {
      CombineStepEvents(ConvertDeviceTraceXLineToStepEvents(line), &result);
    }
  });
  return result;
}

}  // namespace profiler
}  // namespace tensorflow
