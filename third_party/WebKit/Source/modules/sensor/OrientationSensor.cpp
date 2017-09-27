// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/OrientationSensor.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/geometry/DOMMatrix.h"

using device::mojom::blink::SensorType;

namespace blink {

Vector<double> OrientationSensor::quaternion(bool& is_null) {
  reading_dirty_ = false;
  INIT_IS_NULL_AND_RETURN(is_null, Vector<double>());
  const auto& quat = proxy()->reading().orientation_quat;
  return Vector<double>({quat.x, quat.y, quat.z, quat.w});
}

template <typename T>
void DoPopulateMatrix(T* target_matrix,
                      double x,
                      double y,
                      double z,
                      double w) {
  auto out = target_matrix->Data();
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
void DoPopulateMatrix(DOMMatrix* target_matrix,
                      double x,
                      double y,
                      double z,
                      double w) {
  target_matrix->setM11(1.0 - 2 * (y * y + z * z));
  target_matrix->setM12(2 * (x * y - z * w));
  target_matrix->setM13(2 * (x * z + y * w));
  target_matrix->setM14(0.0);
  target_matrix->setM21(2 * (x * y + z * w));
  target_matrix->setM22(1.0 - 2 * (x * x + z * z));
  target_matrix->setM23(2 * y * z - 2 * x * w);
  target_matrix->setM24(0.0);
  target_matrix->setM31(2 * (x * z - y * w));
  target_matrix->setM32(2 * (y * z + x * w));
  target_matrix->setM33(1.0 - 2 * (x * x + y * y));
  target_matrix->setM34(0.0);
  target_matrix->setM41(0.0);
  target_matrix->setM42(0.0);
  target_matrix->setM43(0.0);
  target_matrix->setM44(1.0);
}

template <typename T>
bool CheckBufferLength(T* buffer) {
  return buffer->length() >= 16;
}

template <>
bool CheckBufferLength(DOMMatrix*) {
  return true;
}

template <typename Matrix>
void OrientationSensor::PopulateMatrixInternal(
    Matrix* target_matrix,
    ExceptionState& exception_state) {
  if (!CheckBufferLength(target_matrix)) {
    exception_state.ThrowTypeError(
        "Target buffer must have at least 16 elements.");
    return;
  }
  if (!hasReading()) {
    exception_state.ThrowDOMException(kNotReadableError,
                                      "Sensor data is not available.");
    return;
  }

  const auto& quat = proxy()->reading().orientation_quat;

  DoPopulateMatrix(target_matrix, quat.x, quat.y, quat.z, quat.w);
}

void OrientationSensor::populateMatrix(
    Float32ArrayOrFloat64ArrayOrDOMMatrix& matrix,
    ExceptionState& exception_state) {
  if (matrix.isFloat32Array())
    PopulateMatrixInternal(matrix.getAsFloat32Array().View(), exception_state);
  else if (matrix.isFloat64Array())
    PopulateMatrixInternal(matrix.getAsFloat64Array().View(), exception_state);
  else if (matrix.isDOMMatrix())
    PopulateMatrixInternal(matrix.getAsDOMMatrix(), exception_state);
  else
    NOTREACHED() << "Unexpected rotation matrix type.";
}

bool OrientationSensor::isReadingDirty() const {
  return reading_dirty_ || !hasReading();
}

OrientationSensor::OrientationSensor(ExecutionContext* execution_context,
                                     const SensorOptions& options,
                                     ExceptionState& exception_state,
                                     device::mojom::blink::SensorType type)
    : Sensor(execution_context, options, exception_state, type),
      reading_dirty_(true) {}

void OrientationSensor::OnSensorReadingChanged() {
  reading_dirty_ = true;
  Sensor::OnSensorReadingChanged();
}

DEFINE_TRACE(OrientationSensor) {
  Sensor::Trace(visitor);
}

}  // namespace blink
