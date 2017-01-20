// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/TaskAttributionTiming.h"

#include "core/frame/DOMWindow.h"

namespace blink {

TaskAttributionTiming::TaskAttributionTiming(String name,
                                             String containerType,
                                             String containerSrc,
                                             String containerId,
                                             String containerName)
    : PerformanceEntry(name, "taskattribution", 0.0, 0.0),
      m_containerType(containerType),
      m_containerSrc(containerSrc),
      m_containerId(containerId),
      m_containerName(containerName) {}

TaskAttributionTiming::~TaskAttributionTiming() {}

String TaskAttributionTiming::containerType() const {
  return m_containerType;
}

String TaskAttributionTiming::containerSrc() const {
  return m_containerSrc;
}

String TaskAttributionTiming::containerId() const {
  return m_containerId;
}

String TaskAttributionTiming::containerName() const {
  return m_containerName;
}

DEFINE_TRACE(TaskAttributionTiming) {
  PerformanceEntry::trace(visitor);
}

}  // namespace blink
