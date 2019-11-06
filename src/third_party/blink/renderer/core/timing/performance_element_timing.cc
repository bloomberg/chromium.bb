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
    const AtomicString& id,
    Element* element) {
  // It is possible to 'paint' images which have naturalWidth or naturalHeight
  // equal to 0.
  DCHECK_GE(naturalWidth, 0);
  DCHECK_GE(naturalHeight, 0);
  DCHECK(element);
  return MakeGarbageCollected<PerformanceElementTiming>(
      name, intersection_rect, start_time, response_end, identifier,
      naturalWidth, naturalHeight, id, element);
}

PerformanceElementTiming::PerformanceElementTiming(
    const AtomicString& name,
    const FloatRect& intersection_rect,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp response_end,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id,
    Element* element)
    : PerformanceEntry(name, start_time, start_time),
      element_(element),
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

Element* PerformanceElementTiming::element() const {
  if (!element_ || !element_->isConnected() || element_->IsInShadowTree())
    return nullptr;

  return element_;
}

void PerformanceElementTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("intersectionRect", intersection_rect_);
}

void PerformanceElementTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(intersection_rect_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
