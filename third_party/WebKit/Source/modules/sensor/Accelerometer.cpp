// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Accelerometer.h"

using device::mojom::blink::SensorType;

namespace blink {

Accelerometer* Accelerometer::Create(ExecutionContext* execution_context,
                                     const SensorOptions& options,
                                     ExceptionState& exception_state) {
  return new Accelerometer(execution_context, options, exception_state,
                           SensorType::ACCELEROMETER);
}

// static
Accelerometer* Accelerometer::Create(ExecutionContext* execution_context,
                                     ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions(), exception_state);
}

Accelerometer::Accelerometer(ExecutionContext* execution_context,
                             const SensorOptions& options,
                             ExceptionState& exception_state,
                             SensorType sensor_type)
    : Sensor(execution_context, options, exception_state, sensor_type) {}

double Accelerometer::x(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().accel.x;
}

double Accelerometer::y(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().accel.y;
}

double Accelerometer::z(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().accel.z;
}

DEFINE_TRACE(Accelerometer) {
  Sensor::Trace(visitor);
}

}  // namespace blink
