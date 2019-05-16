// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

// Exposes the layout shift score of an animation frame, computed as described
// in http://bit.ly/lsm-explainer.
class CORE_EXPORT LayoutShift final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LayoutShift(double start_time, double value);
  ~LayoutShift() override;

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  double value() const { return value_; }

  void Trace(blink::Visitor*) override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  double value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_
