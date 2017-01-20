// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"

#include "core/frame/DOMWindow.h"
#include "core/timing/TaskAttributionTiming.h"

namespace blink {

namespace {

double clampToMillisecond(double timeInMillis) {
  // Long task times are clamped to 1 millisecond for security.
  return floor(timeInMillis);
}

}  // namespace

// static
PerformanceLongTaskTiming* PerformanceLongTaskTiming::create(double startTime,
                                                             double endTime,
                                                             String name,
                                                             String frameSrc,
                                                             String frameId,
                                                             String frameName) {
  return new PerformanceLongTaskTiming(startTime, endTime, name, frameSrc,
                                       frameId, frameName);
}

PerformanceLongTaskTiming::PerformanceLongTaskTiming(double startTime,
                                                     double endTime,
                                                     String name,
                                                     String culpritFrameSrc,
                                                     String culpritFrameId,
                                                     String culpritFrameName)
    : PerformanceEntry(name,
                       "longtask",
                       clampToMillisecond(startTime),
                       clampToMillisecond(endTime)) {
  // Only one possible task type exists currently: "script"
  // Only one possible container type exists currently: "iframe"
  TaskAttributionTiming* attributionEntry = TaskAttributionTiming::create(
      "script", "iframe", culpritFrameSrc, culpritFrameId, culpritFrameName);
  m_attribution.push_back(*attributionEntry);
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() {}

TaskAttributionVector PerformanceLongTaskTiming::attribution() const {
  return m_attribution;
}

DEFINE_TRACE(PerformanceLongTaskTiming) {
  visitor->trace(m_attribution);
  PerformanceEntry::trace(visitor);
}

}  // namespace blink
