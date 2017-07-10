// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AbsoluteOrientationSensor.h"

using device::mojom::blink::SensorType;

namespace blink {

AbsoluteOrientationSensor* AbsoluteOrientationSensor::Create(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state) {
  return new AbsoluteOrientationSensor(execution_context, options,
                                       exception_state);
}

// static
AbsoluteOrientationSensor* AbsoluteOrientationSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions(), exception_state);
}

AbsoluteOrientationSensor::AbsoluteOrientationSensor(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state)
    : OrientationSensor(execution_context,
                        options,
                        exception_state,
                        SensorType::ABSOLUTE_ORIENTATION_QUATERNION) {}

DEFINE_TRACE(AbsoluteOrientationSensor) {
  OrientationSensor::Trace(visitor);
}

}  // namespace blink
