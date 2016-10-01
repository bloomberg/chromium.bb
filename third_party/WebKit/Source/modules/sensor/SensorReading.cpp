// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReading.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"

namespace blink {

SensorReading::SensorReading(SensorProxy* sensorProxy)
    : m_sensorProxy(sensorProxy) {}

SensorReading::~SensorReading() = default;

DEFINE_TRACE(SensorReading) {
  visitor->trace(m_sensorProxy);
}

DOMHighResTimeStamp SensorReading::timeStamp(ScriptState* scriptState) const {
  LocalDOMWindow* window = scriptState->domWindow();
  if (!window)
    return 0.0;

  Performance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  if (!m_sensorProxy) {
    // In cases when SensorReading derived classes are constructed from JS
    // side, e.g. to create syntetic SensorReadingEvent for testing
    // purposes, |m_sensorProxy| will be null and SensorReading.timeStamp
    // would return current DOMHighResTimeStamp, while reading value should
    // be provided by derived classes.
    return performance->now();
  }

  return performance->monotonicTimeToDOMHighResTimeStamp(
      m_sensorProxy->reading().timestamp);
}

}  // namespace blink
