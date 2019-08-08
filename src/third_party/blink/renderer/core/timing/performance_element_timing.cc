// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_element_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

// static
PerformanceElementTiming* PerformanceElementTiming::Create(
    const AtomicString& name,
    const FloatRect& intersection_rect,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp response_end,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id) {
  DCHECK_GT(naturalWidth, 0);
  DCHECK_GT(naturalHeight, 0);
  return MakeGarbageCollected<PerformanceElementTiming>(
      name, intersection_rect, start_time, response_end, identifier,
      naturalWidth, naturalHeight, id);
}

PerformanceElementTiming::PerformanceElementTiming(
    const AtomicString& name,
    const FloatRect& intersection_rect,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp response_end,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id)
    : PerformanceEntry(name, start_time, start_time),
      intersection_rect_(DOMRectReadOnly::FromFloatRect(intersection_rect)),
      response_end_(response_end),
      identifier_(identifier),
      naturalWidth_(naturalWidth),
      naturalHeight_(naturalHeight),
      id_(id) {}

PerformanceElementTiming::~PerformanceElementTiming() = default;

AtomicString PerformanceElementTiming::entryType() const {
  return performance_entry_names::kElement;
}

PerformanceEntryType PerformanceElementTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kElement;
}

void PerformanceElementTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("intersectionRect", intersection_rect_);
}

void PerformanceElementTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(intersection_rect_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
