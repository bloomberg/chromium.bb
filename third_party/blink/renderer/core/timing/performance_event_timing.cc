// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_event_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

// static
PerformanceEventTiming* PerformanceEventTiming::Create(
    String type,
    TimeDelta start_time,
    TimeDelta processing_start,
    TimeDelta end_time,
    bool cancelable) {
  return new PerformanceEventTiming(type, start_time, processing_start,
                                    end_time, cancelable);
}

// static
DOMHighResTimeStamp PerformanceEventTiming::timeDeltaToDOMHighResTimeStamp(
    TimeDelta time_delta) {
  double time = time_delta.InSecondsF();
  if (time < 0)
    time = 0.0;
  return ConvertSecondsToDOMHighResTimeStamp(
      Performance::ClampTimeResolution(time));
}

PerformanceEventTiming::PerformanceEventTiming(String type,
                                               TimeDelta start_time,
                                               TimeDelta processing_start,
                                               TimeDelta end_time,
                                               bool cancelable)
    : PerformanceEntry(type,
                       "event",
                       timeDeltaToDOMHighResTimeStamp(start_time),
                       timeDeltaToDOMHighResTimeStamp(end_time)),
      processing_start_(processing_start),
      cancelable_(cancelable) {}

PerformanceEventTiming::~PerformanceEventTiming() = default;

DOMHighResTimeStamp PerformanceEventTiming::processingStart() const {
  return timeDeltaToDOMHighResTimeStamp(processing_start_);
}

void PerformanceEventTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("processingStart", processingStart());
  builder.AddBoolean("cancelable", cancelable_);
}

void PerformanceEventTiming::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
