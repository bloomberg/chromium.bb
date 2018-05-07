// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

class CORE_EXPORT PerformanceEventTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceEventTiming* Create(String type,
                                        TimeDelta start_time,
                                        TimeDelta processing_start,
                                        TimeDelta end_time,
                                        bool cancelable);
  static DOMHighResTimeStamp timeDeltaToDOMHighResTimeStamp(TimeDelta);

  ~PerformanceEventTiming() override;

  bool cancelable() const { return cancelable_; }

  DOMHighResTimeStamp processingStart() const;

  void BuildJSONValue(V8ObjectBuilder&) const override;

  void Trace(blink::Visitor*) override;

 private:
  PerformanceEventTiming(String type,
                         TimeDelta start_time,
                         TimeDelta processing_start,
                         TimeDelta end_time,
                         bool cancelable);
  TimeDelta processing_start_;
  bool cancelable_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
