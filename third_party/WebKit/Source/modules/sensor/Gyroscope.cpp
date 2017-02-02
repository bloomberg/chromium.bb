// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Gyroscope.h"


using device::mojom::blink::SensorType;

namespace blink {

Gyroscope* Gyroscope::create(ExecutionContext* executionContext,
                             const SensorOptions& options,
                             ExceptionState& exceptionState) {
  return new Gyroscope(executionContext, options, exceptionState);
}

// static
Gyroscope* Gyroscope::create(ExecutionContext* executionContext,
                             ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

Gyroscope::Gyroscope(ExecutionContext* executionContext,
                     const SensorOptions& options,
                     ExceptionState& exceptionState)
    : Sensor(executionContext, options, exceptionState, SensorType::GYROSCOPE) {
}

double Gyroscope::x(bool& isNull) const {
  return readingValue(0, isNull);
}

double Gyroscope::y(bool& isNull) const {
  return readingValue(1, isNull);
}

double Gyroscope::z(bool& isNull) const {
  return readingValue(2, isNull);
}

DEFINE_TRACE(Gyroscope) {
  Sensor::trace(visitor);
}

}  // namespace blink
