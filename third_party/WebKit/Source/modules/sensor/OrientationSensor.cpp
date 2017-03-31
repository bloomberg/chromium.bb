// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/OrientationSensor.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/geometry/DOMMatrix.h"

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

template <typename T>
void doPopulateMatrix(T* targetMatrix, double x, double y, double z, double w) {
  auto out = targetMatrix->data();
  out[0] = 1.0 - 2 * (y * y + z * z);
  out[1] = 2 * (x * y - z * w);
  out[2] = 2 * (x * z + y * w);
  out[3] = 0.0;
  out[4] = 2 * (x * y + z * w);
  out[5] = 1.0 - 2 * (x * x + z * z);
  out[6] = 2 * (y * z - x * w);
  out[7] = 0.0;
  out[8] = 2 * (x * z - y * w);
  out[9] = 2 * (y * z + x * w);
  out[10] = 1.0 - 2 * (x * x + y * y);
  out[11] = 0.0;
  out[12] = 0.0;
  out[13] = 0.0;
  out[14] = 0.0;
  out[15] = 1.0;
}

template <>
void doPopulateMatrix(DOMMatrix* targetMatrix,
                      double x,
                      double y,
                      double z,
                      double w) {
  targetMatrix->setM11(1.0 - 2 * (y * y + z * z));
  targetMatrix->setM12(2 * (x * y - z * w));
  targetMatrix->setM13(2 * (x * z + y * w));
  targetMatrix->setM14(0.0);
  targetMatrix->setM21(2 * (x * y + z * w));
  targetMatrix->setM22(1.0 - 2 * (x * x + z * z));
  targetMatrix->setM23(2 * y * z - 2 * x * w);
  targetMatrix->setM24(0.0);
  targetMatrix->setM31(2 * (x * z - y * w));
  targetMatrix->setM32(2 * (y * z + x * w));
  targetMatrix->setM33(1.0 - 2 * (x * x + y * y));
  targetMatrix->setM34(0.0);
  targetMatrix->setM41(0.0);
  targetMatrix->setM42(0.0);
  targetMatrix->setM43(0.0);
  targetMatrix->setM44(1.0);
}

template <typename T>
bool checkBufferLength(T* buffer) {
  return buffer->length() >= 16;
}

template <>
bool checkBufferLength(DOMMatrix*) {
  return true;
}

template <typename Matrix>
void OrientationSensor::populateMatrixInternal(Matrix* targetMatrix,
                                               ExceptionState& exceptionState) {
  if (!checkBufferLength(targetMatrix)) {
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

  double x = readingValueUnchecked(0);
  double y = readingValueUnchecked(1);
  double z = readingValueUnchecked(2);
  double w = readingValueUnchecked(3);

  doPopulateMatrix(targetMatrix, x, y, z, w);
}

void OrientationSensor::populateMatrix(
    Float32ArrayOrFloat64ArrayOrDOMMatrix& matrix,
    ExceptionState& exceptionState) {
  if (matrix.isFloat32Array())
    populateMatrixInternal(matrix.getAsFloat32Array(), exceptionState);
  else if (matrix.isFloat64Array())
    populateMatrixInternal(matrix.getAsFloat64Array(), exceptionState);
  else if (matrix.isDOMMatrix())
    populateMatrixInternal(matrix.getAsDOMMatrix(), exceptionState);
  else
    NOTREACHED() << "Unexpected rotation matrix type.";
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
