// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"
#include "core/frame/DOMWindow.h"
#include "core/timing/SubTaskAttribution.h"
#include "core/timing/TaskAttributionTiming.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

// static
PerformanceLongTaskTiming* PerformanceLongTaskTiming::Create(
    double start_time,
    double end_time,
    String name,
    String frame_src,
    String frame_id,
    String frame_name,
    const SubTaskAttribution::EntriesVector& sub_task_attributions) {
  return new PerformanceLongTaskTiming(start_time, end_time, name, frame_src,
                                       frame_id, frame_name,
                                       sub_task_attributions);
}

PerformanceLongTaskTiming::PerformanceLongTaskTiming(
    double start_time,
    double end_time,
    String name,
    String culprit_frame_src,
    String culprit_frame_id,
    String culprit_frame_name,
    const SubTaskAttribution::EntriesVector& sub_task_attributions)
    : PerformanceEntry(name, "longtask", start_time, end_time) {
  // Only one possible container type exists currently: "iframe".
  if (RuntimeEnabledFeatures::LongTaskV2Enabled()) {
    for (auto&& it : sub_task_attributions) {
      TaskAttributionTiming* attribution_entry = TaskAttributionTiming::Create(
          it->subTaskName(), "iframe", culprit_frame_src, culprit_frame_id,
          culprit_frame_name, it->highResStartTime(),
          it->highResStartTime() + it->highResDuration(), it->scriptURL());
      attribution_.push_back(*attribution_entry);
    }
  } else {
    // Only one possible task type exists currently: "script".
    TaskAttributionTiming* attribution_entry =
        TaskAttributionTiming::Create("script", "iframe", culprit_frame_src,
                                      culprit_frame_id, culprit_frame_name);
    attribution_.push_back(*attribution_entry);
  }
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() {}

TaskAttributionVector PerformanceLongTaskTiming::attribution() const {
  return attribution_;
}

void PerformanceLongTaskTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(attribution_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
