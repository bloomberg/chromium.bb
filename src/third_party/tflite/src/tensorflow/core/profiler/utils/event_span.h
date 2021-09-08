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

#ifndef TENSORFLOW_CORE_PROFILER_UTILS_EVENT_SPAN_H_
#define TENSORFLOW_CORE_PROFILER_UTILS_EVENT_SPAN_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/protobuf/op_metrics.pb.h"
#include "tensorflow/core/profiler/utils/timespan.h"

namespace tensorflow {
namespace profiler {

// The various event types. Enumerations are numbered such that a bigger number
// has a higher priority than a smaller number when used in execution-time
// breakdown.
enum EventType {
  // No event associated with the time. It could be that the machine was idle or
  // executing some events which were not traced.
  UNKNOWN_TIME = 0,
  // Host is computing.
  HOST_COMPUTE = 1,
  // Host is compiling.
  HOST_COMPILE = 2,
  // Host-to-host communication.
  HOST_TO_HOST = 3,
  // Host-to-device communication.
  HOST_TO_DEVICE = 4,
  // Host is preparing to launch a computation on device.
  HOST_PREPARE = 5,
  // Host is waiting for input.
  HOST_WAIT_INPUT = 6,
  // Device-to-device communication.
  DEVICE_TO_DEVICE = 7,
  // Device-to-host communication.
  DEVICE_TO_HOST = 8,
  // Device is computing with 32-bit precision.
  DEVICE_COMPUTE_32 = 9,
  // Device is computing with 16-bit precision.
  DEVICE_COMPUTE_16 = 10,
  // Device is waiting for another device.
  DEVICE_WAIT_DEVICE = 11,
  // Device is waiting for host.
  DEVICE_WAIT_HOST = 12,
  LAST_EVENT_TYPE = DEVICE_WAIT_HOST
};

// Contains the type and timespan of an event.
struct EventTypeSpan {
  EventType type;  // type of this event.
  Timespan span;   // timespan of this event.
  EventTypeSpan(EventType t, Timespan s) : type(t), span(s) {}
  // Equality test.
  bool operator==(const EventTypeSpan& other) const {
    return type == other.type && span == other.span;
  }
  // Inequality test.
  bool operator!=(const EventTypeSpan& other) const {
    return !(*this == other);
  }
};

enum class StepMarkerType {
  // "TraceContext" TraceMe events.
  kExplicitHostStepMarker,
  // Identified by group_events (e.g., FunctionRun, SessionRun).
  kImplicitHostStepMarker,
  // Derived from the result of group_events. A device step marker starts with
  // the first device event of the group and ends with the last event of the
  // group.
  kDeviceStepMarker,
};

// Record of an event that is used as a step marker.
struct StepMarker {
  StepMarkerType type;
  std::string event_name;  // name of this event.
  Timespan span;           // timespan of this event.
  StepMarker(StepMarkerType step_marker_type, absl::string_view name,
             Timespan s)
      : type(step_marker_type), event_name(name), span(s) {}
  // Equality test.
  bool operator==(const StepMarker& other) const {
    return type == other.type && event_name == other.event_name &&
           span == other.span;
  }
  // Inequality test.
  bool operator!=(const StepMarker& other) const { return !(*this == other); }
};

// Details of a step. Note that this could be the result of combining the
// StepDetails of the same step executed on different cores.
class StepDetails {
 private:
  // All step-markers found for marking this step in the traces. There could be
  // multiple step-markers for a single step for different reasons. One such
  // reason is that there may be one step-marker for the same step on each core;
  // so after combining the StepDetails from multiple cores, there would be
  // multiple step-markers for the same step.
  std::vector<StepMarker> markers_;
  // All events belonging to this step.
  std::vector<EventTypeSpan> events_;

 public:
  const std::vector<StepMarker>& Markers() const { return markers_; }
  const std::vector<EventTypeSpan>& Events() const { return events_; }
  // Returns the step time.
  Timespan StepTime() const;
  std::vector<StepMarker>* MutableMarkers() { return &markers_; }
  std::vector<EventTypeSpan>* MutableEvents() { return &events_; }
  // Adds a step-marker to this step.
  void AddMarker(const StepMarker& m);
  // Adds an EventTypeSpan to this step.
  void AddEvent(const EventTypeSpan& e);
  // Appends the step-markers from another step to this step.
  void AppendMarkers(const std::vector<StepMarker>& other_markers);
  // Appends the events from another step to this step.
  void AppendEvents(const std::vector<EventTypeSpan>& other_events);
  // Equality test.
  bool operator==(const StepDetails& other) const;
  // Inequality test.
  bool operator!=(const StepDetails& other) const { return !(*this == other); }
  // Returns a string that prints the content of this object.
  std::string DebugString() const;
};

// Map from step_id to the events happened in that step.
using StepEvents = absl::flat_hash_map<int64 /*step_id*/, StepDetails>;

// Equality test for StepEvents.
bool operator==(const StepEvents& a, const StepEvents& b);

// Returns the event type of the given CPU event.
EventType ClassifyCpuEvent(absl::string_view event_name, int64 correlation_id,
                           bool has_device);

// Returns the event type of the given GPU event and tensor shapes.
EventType ClassifyGpuEvent(absl::string_view event_name,
                           absl::string_view tensor_shapes);

// Returns the name of the given EventType.
std::string PrintEventType(EventType event_type);

// Returns a string that prints the given EventTypeSpan.
std::string PrintEventTypeSpan(const EventTypeSpan& event_type_span);

// Returns a string that prints the given StepMarker.
std::string PrintStepMarker(const StepMarker& step_marker);

// Returns a string that prints the given StepEvents.
std::string PrintStepEvents(const StepEvents& step_events);

// Combines the src StepEvents into dst.
void CombineStepEvents(const StepEvents& src, StepEvents* dst);

// Converts from overlapped step-events to non-overlapped step events.
StepEvents ToNonOverlappedStepEvents(const StepEvents& overlapped_step_events);

// Returns the precision stats of the given non-overlapped step events.
PrecisionStats ComputePrecisionStats(
    const StepEvents& nonoverlapped_step_events);

}  // namespace profiler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PROFILER_UTILS_EVENT_SPAN_H_
