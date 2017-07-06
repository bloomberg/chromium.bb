// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/LinearAccelerationSensor.h"

using device::mojom::blink::SensorType;

namespace blink {

LinearAccelerationSensor* LinearAccelerationSensor::Create(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state) {
  return new LinearAccelerationSensor(execution_context, options,
                                      exception_state);
}

// static
LinearAccelerationSensor* LinearAccelerationSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions(), exception_state);
}

LinearAccelerationSensor::LinearAccelerationSensor(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state)
    : Accelerometer(execution_context,
                    options,
                    exception_state,
                    SensorType::LINEAR_ACCELERATION) {}

DEFINE_TRACE(LinearAccelerationSensor) {
  Accelerometer::Trace(visitor);
}

}  // namespace blink
