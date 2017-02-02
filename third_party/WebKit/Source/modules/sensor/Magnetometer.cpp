// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Magnetometer.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
Magnetometer* Magnetometer::create(ExecutionContext* executionContext,
                                   const SensorOptions& options,
                                   ExceptionState& exceptionState) {
  return new Magnetometer(executionContext, options, exceptionState);
}

// static
Magnetometer* Magnetometer::create(ExecutionContext* executionContext,
                                   ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

Magnetometer::Magnetometer(ExecutionContext* executionContext,
                           const SensorOptions& options,
                           ExceptionState& exceptionState)
    : Sensor(executionContext,
             options,
             exceptionState,
             SensorType::MAGNETOMETER) {}

double Magnetometer::x(bool& isNull) const {
  return readingValue(0, isNull);
}

double Magnetometer::y(bool& isNull) const {
  return readingValue(1, isNull);
}

double Magnetometer::z(bool& isNull) const {
  return readingValue(2, isNull);
}

DEFINE_TRACE(Magnetometer) {
  Sensor::trace(visitor);
}

}  // namespace blink
