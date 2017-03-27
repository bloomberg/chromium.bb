// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AbsoluteOrientationSensor.h"

using device::mojom::blink::SensorType;

namespace blink {

AbsoluteOrientationSensor* AbsoluteOrientationSensor::create(
    ExecutionContext* executionContext,
    const SensorOptions& options,
    ExceptionState& exceptionState) {
  return new AbsoluteOrientationSensor(executionContext, options,
                                       exceptionState);
}

// static
AbsoluteOrientationSensor* AbsoluteOrientationSensor::create(
    ExecutionContext* executionContext,
    ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

AbsoluteOrientationSensor::AbsoluteOrientationSensor(
    ExecutionContext* executionContext,
    const SensorOptions& options,
    ExceptionState& exceptionState)
    : OrientationSensor(executionContext,
                        options,
                        exceptionState,
                        SensorType::ABSOLUTE_ORIENTATION) {}

DEFINE_TRACE(AbsoluteOrientationSensor) {
  OrientationSensor::trace(visitor);
}

}  // namespace blink
