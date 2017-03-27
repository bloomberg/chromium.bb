// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/OrientationSensor.h"

#include "bindings/core/v8/ExceptionState.h"

using device::mojom::blink::SensorType;

namespace blink {

Vector<double> OrientationSensor::quaternion(bool& isNull) {
  m_readingDirty = false;
  isNull = !canReturnReadings();
  return isNull ? Vector<double>()
                : Vector<double>({readingValueUnchecked(3),    // W
                                  readingValueUnchecked(0),    // Vx
                                  readingValueUnchecked(1),    // Vy
                                  readingValueUnchecked(2)});  // Vz
}

void OrientationSensor::populateMatrix(DOMFloat32Array* buffer,
                                       ExceptionState& exceptionState) {
  if (buffer->length() < 16) {
    exceptionState.throwTypeError(
        "Target buffer must have at least 16 elements.");
    return;
  }
  if (!isActivated()) {
    exceptionState.throwDOMException(
        InvalidStateError, "The sensor must be in 'connected' state.");
    return;
  }
  if (!canReturnReadings())
    return;

  float x = readingValueUnchecked(0);
  float y = readingValueUnchecked(1);
  float z = readingValueUnchecked(2);
  float w = readingValueUnchecked(3);

  float* out = buffer->data();
  out[0] = 1.0 - 2 * (y * y - z * z);
  out[1] = 2 * (x * y - z * w);
  out[2] = 2 * (x * z + y * w);
  out[4] = 2 * (x * y + z * w);
  out[5] = 1.0 - 2 * (x * x - z * z);
  out[6] = 2 * (y * z - x * w);
  out[8] = 2 * (x * z - y * w);
  out[9] = 2 * (y * z + x * w);
  out[10] = 1.0 - 2 * (x * x - y * y);
  out[15] = 1.0;
}

bool OrientationSensor::isReadingDirty() const {
  return m_readingDirty || !canReturnReadings();
}

OrientationSensor::OrientationSensor(ExecutionContext* executionContext,
                                     const SensorOptions& options,
                                     ExceptionState& exceptionState,
                                     device::mojom::blink::SensorType type)
    : Sensor(executionContext, options, exceptionState, type),
      m_readingDirty(true) {}

void OrientationSensor::onSensorReadingChanged() {
  m_readingDirty = true;
}

DEFINE_TRACE(OrientationSensor) {
  Sensor::trace(visitor);
}

}  // namespace blink
