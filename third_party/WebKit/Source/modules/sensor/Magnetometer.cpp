// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Magnetometer.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
Magnetometer* Magnetometer::Create(ExecutionContext* execution_context,
                                   const SensorOptions& options,
                                   ExceptionState& exception_state) {
  return new Magnetometer(execution_context, options, exception_state);
}

// static
Magnetometer* Magnetometer::Create(ExecutionContext* execution_context,
                                   ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions(), exception_state);
}

Magnetometer::Magnetometer(ExecutionContext* execution_context,
                           const SensorOptions& options,
                           ExceptionState& exception_state)
    : Sensor(execution_context,
             options,
             exception_state,
             SensorType::MAGNETOMETER) {}

double Magnetometer::x(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().magn.x;
}

double Magnetometer::y(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().magn.y;
}

double Magnetometer::z(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return proxy()->reading().magn.z;
}

void Magnetometer::Trace(blink::Visitor* visitor) {
  Sensor::Trace(visitor);
}

}  // namespace blink
