// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Accelerometer_h
#define Accelerometer_h

#include "modules/sensor/Sensor.h"

namespace blink {

class Accelerometer : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Accelerometer* Create(ExecutionContext*,
                               const SensorOptions&,
                               ExceptionState&);
  static Accelerometer* Create(ExecutionContext*, ExceptionState&);

  double x(bool& is_null) const;
  double y(bool& is_null) const;
  double z(bool& is_null) const;

  virtual void Trace(blink::Visitor*);

 protected:
  Accelerometer(ExecutionContext*,
                const SensorOptions&,
                ExceptionState&,
                device::mojom::blink::SensorType);
};

}  // namespace blink

#endif  // Accelerometer_h
