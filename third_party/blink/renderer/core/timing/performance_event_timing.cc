// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_event_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

// static
PerformanceEventTiming* PerformanceEventTiming::Create(
    const String& type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(start_time, processing_start);
  DCHECK_LE(processing_start, processing_end);
  return new PerformanceEventTiming(type, start_time, processing_start,
                                    processing_end, cancelable);
}

PerformanceEventTiming::PerformanceEventTiming(
    const String& type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable)
    : PerformanceEntry(type, "event", start_time, 0.0),
      processing_start_(processing_start),
      processing_end_(processing_end),
      cancelable_(cancelable) {}

PerformanceEventTiming::~PerformanceEventTiming() = default;

DOMHighResTimeStamp PerformanceEventTiming::processingStart() const {
  return processing_start_;
}

DOMHighResTimeStamp PerformanceEventTiming::processingEnd() const {
  return processing_end_;
}

void PerformanceEventTiming::SetDuration(double duration) {
  DCHECK_LE(0, duration);
  duration_ = duration;
}

void PerformanceEventTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("processingStart", processingStart());
  builder.AddNumber("processingEnd", processingEnd());
  builder.AddBoolean("cancelable", cancelable_);
}

void PerformanceEventTiming::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
