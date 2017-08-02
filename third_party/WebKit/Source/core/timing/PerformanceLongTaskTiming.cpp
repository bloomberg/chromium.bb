// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"

#include "core/frame/DOMWindow.h"
#include "core/timing/TaskAttributionTiming.h"

namespace blink {

// static
PerformanceLongTaskTiming* PerformanceLongTaskTiming::Create(
    double start_time,
    double end_time,
    String name,
    String frame_src,
    String frame_id,
    String frame_name) {
  return new PerformanceLongTaskTiming(start_time, end_time, name, frame_src,
                                       frame_id, frame_name);
}

PerformanceLongTaskTiming::PerformanceLongTaskTiming(double start_time,
                                                     double end_time,
                                                     String name,
                                                     String culprit_frame_src,
                                                     String culprit_frame_id,
                                                     String culprit_frame_name)
    : PerformanceEntry(name, "longtask", start_time, end_time) {
  // Only one possible task type exists currently: "script"
  // Only one possible container type exists currently: "iframe"
  TaskAttributionTiming* attribution_entry =
      TaskAttributionTiming::Create("script", "iframe", culprit_frame_src,
                                    culprit_frame_id, culprit_frame_name);
  attribution_.push_back(*attribution_entry);
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() {}

TaskAttributionVector PerformanceLongTaskTiming::attribution() const {
  return attribution_;
}

DEFINE_TRACE(PerformanceLongTaskTiming) {
  visitor->Trace(attribution_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
