// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/TaskAttributionTiming.h"

#include "core/frame/DOMWindow.h"

namespace blink {

TaskAttributionTiming::TaskAttributionTiming(String name,
                                             String container_type,
                                             String container_src,
                                             String container_id,
                                             String container_name)
    : PerformanceEntry(name, "taskattribution", 0.0, 0.0),
      container_type_(container_type),
      container_src_(container_src),
      container_id_(container_id),
      container_name_(container_name) {}

TaskAttributionTiming::~TaskAttributionTiming() {}

String TaskAttributionTiming::containerType() const {
  return container_type_;
}

String TaskAttributionTiming::containerSrc() const {
  return container_src_;
}

String TaskAttributionTiming::containerId() const {
  return container_id_;
}

String TaskAttributionTiming::containerName() const {
  return container_name_;
}

DEFINE_TRACE(TaskAttributionTiming) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
