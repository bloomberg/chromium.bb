// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_event_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
PerformanceEventTiming* PerformanceEventTiming::Create(
    const AtomicString& event_type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable,
    Node* target) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(start_time, processing_start);
  DCHECK_LE(processing_start, processing_end);
  return MakeGarbageCollected<PerformanceEventTiming>(
      event_type, performance_entry_names::kEvent, start_time, processing_start,
      processing_end, cancelable, target);
}

// static
PerformanceEventTiming* PerformanceEventTiming::CreateFirstInputTiming(
    PerformanceEventTiming* entry) {
  PerformanceEventTiming* first_input =
      MakeGarbageCollected<PerformanceEventTiming>(
          entry->name(), performance_entry_names::kFirstInput,
          entry->startTime(), entry->processingStart(), entry->processingEnd(),
          entry->cancelable(), entry->target());
  first_input->SetDuration(entry->duration());
  return first_input;
}

PerformanceEventTiming::PerformanceEventTiming(
    const AtomicString& event_type,
    const AtomicString& entry_type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable,
    Node* target)
    : PerformanceEntry(event_type, start_time, 0.0),
      entry_type_(entry_type),
      processing_start_(processing_start),
      processing_end_(processing_end),
      cancelable_(cancelable),
      target_(target) {}

PerformanceEventTiming::~PerformanceEventTiming() = default;

PerformanceEntryType PerformanceEventTiming::EntryTypeEnum() const {
  return entry_type_ == performance_entry_names::kEvent
             ? PerformanceEntry::EntryType::kEvent
             : PerformanceEntry::EntryType::kFirstInput;
}

DOMHighResTimeStamp PerformanceEventTiming::processingStart() const {
  return processing_start_;
}

DOMHighResTimeStamp PerformanceEventTiming::processingEnd() const {
  return processing_end_;
}

Node* PerformanceEventTiming::target() const {
  return Performance::CanExposeNode(target_) ? target_ : nullptr;
}

void PerformanceEventTiming::SetDuration(double duration) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(0, duration);
  duration_ = duration;
}

void PerformanceEventTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("processingStart", processingStart());
  builder.AddNumber("processingEnd", processingEnd());
  builder.AddBoolean("cancelable", cancelable_);
  builder.Add("target", target());
}

void PerformanceEventTiming::Trace(Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(target_);
}

}  // namespace blink
