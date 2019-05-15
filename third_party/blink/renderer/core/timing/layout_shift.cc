// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LayoutShift::LayoutShift(double start_time, double fraction)
    : PerformanceEntry(g_empty_atom, start_time, start_time),
      fraction_(fraction) {}

LayoutShift::~LayoutShift() = default;

AtomicString LayoutShift::entryType() const {
  return performance_entry_names::kLayoutJank;
}

PerformanceEntryType LayoutShift::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLayoutJank;
}

void LayoutShift::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("fraction", fraction_);
}

void LayoutShift::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
