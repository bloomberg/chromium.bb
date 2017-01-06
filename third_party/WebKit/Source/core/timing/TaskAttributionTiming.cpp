// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/TaskAttributionTiming.h"

#include "core/frame/DOMWindow.h"

namespace blink {

TaskAttributionTiming::TaskAttributionTiming(String name,
                                             String frameSrc,
                                             String frameId,
                                             String frameName)
    : PerformanceEntry(name, "taskattribution", 0.0, 0.0),
      m_frameSrc(frameSrc),
      m_frameId(frameId),
      m_frameName(frameName) {}

TaskAttributionTiming::~TaskAttributionTiming() {}

String TaskAttributionTiming::frameSrc() const {
  return m_frameSrc;
}

String TaskAttributionTiming::frameId() const {
  return m_frameId;
}

String TaskAttributionTiming::frameName() const {
  return m_frameName;
}

DEFINE_TRACE(TaskAttributionTiming) {
  PerformanceEntry::trace(visitor);
}

}  // namespace blink
