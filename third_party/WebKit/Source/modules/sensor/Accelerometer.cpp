// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Accelerometer.h"

using device::mojom::blink::SensorType;

namespace blink {

Accelerometer* Accelerometer::create(ExecutionContext* executionContext,
                                     const SensorOptions& options,
                                     ExceptionState& exceptionState) {
  return new Accelerometer(executionContext, options, exceptionState);
}

// static
Accelerometer* Accelerometer::create(ExecutionContext* executionContext,
                                     ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

Accelerometer::Accelerometer(ExecutionContext* executionContext,
                             const SensorOptions& options,
                             ExceptionState& exceptionState)
    : Sensor(executionContext,
             options,
             exceptionState,
             SensorType::ACCELEROMETER) {}

double Accelerometer::x(bool& isNull) const {
  return readingValue(0, isNull);
}

double Accelerometer::y(bool& isNull) const {
  return readingValue(1, isNull);
}

double Accelerometer::z(bool& isNull) const {
  return readingValue(2, isNull);
}

DEFINE_TRACE(Accelerometer) {
  Sensor::trace(visitor);
}

}  // namespace blink
