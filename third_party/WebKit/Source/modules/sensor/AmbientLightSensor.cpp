// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AmbientLightSensor.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
AmbientLightSensor* AmbientLightSensor::create(
    ExecutionContext* executionContext,
    const SensorOptions& options,
    ExceptionState& exceptionState) {
  return new AmbientLightSensor(executionContext, options, exceptionState);
}

// static
AmbientLightSensor* AmbientLightSensor::create(
    ExecutionContext* executionContext,
    ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

AmbientLightSensor::AmbientLightSensor(ExecutionContext* executionContext,
                                       const SensorOptions& options,
                                       ExceptionState& exceptionState)
    : Sensor(executionContext,
             options,
             exceptionState,
             SensorType::AMBIENT_LIGHT) {}

double AmbientLightSensor::illuminance(bool& isNull) const {
  return readingValue(0, isNull);
}

DEFINE_TRACE(AmbientLightSensor) {
  Sensor::trace(visitor);
}

}  // namespace blink
