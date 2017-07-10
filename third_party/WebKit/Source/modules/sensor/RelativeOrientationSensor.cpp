// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/RelativeOrientationSensor.h"

using device::mojom::blink::SensorType;

namespace blink {

RelativeOrientationSensor* RelativeOrientationSensor::Create(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state) {
  return new RelativeOrientationSensor(execution_context, options,
                                       exception_state);
}

// static
RelativeOrientationSensor* RelativeOrientationSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions(), exception_state);
}

RelativeOrientationSensor::RelativeOrientationSensor(
    ExecutionContext* execution_context,
    const SensorOptions& options,
    ExceptionState& exception_state)
    : OrientationSensor(execution_context,
                        options,
                        exception_state,
                        SensorType::RELATIVE_ORIENTATION_QUATERNION) {}

DEFINE_TRACE(RelativeOrientationSensor) {
  OrientationSensor::Trace(visitor);
}

}  // namespace blink
