// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"

#include "core/frame/DOMWindow.h"

namespace blink {

PerformanceLongTaskTiming::PerformanceLongTaskTiming(double startTime,
                                                     double endTime,
                                                     String name,
                                                     String culpritFrameSrc,
                                                     String culpritFrameId,
                                                     String culpritFrameName)
    : PerformanceEntry(name, "longtask", startTime, endTime),
      m_culpritFrameSrc(culpritFrameSrc),
      m_culpritFrameId(culpritFrameId),
      m_culpritFrameName(culpritFrameName) {}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() {}

String PerformanceLongTaskTiming::culpritFrameSrc() const {
  return m_culpritFrameSrc;
}

String PerformanceLongTaskTiming::culpritFrameId() const {
  return m_culpritFrameId;
}

String PerformanceLongTaskTiming::culpritFrameName() const {
  return m_culpritFrameName;
}

DEFINE_TRACE(PerformanceLongTaskTiming) {
  PerformanceEntry::trace(visitor);
}

}  // namespace blink
