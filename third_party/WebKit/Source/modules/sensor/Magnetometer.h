// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Magnetometer_h
#define Magnetometer_h

#include "modules/sensor/Sensor.h"

namespace blink {

class Magnetometer final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Magnetometer* Create(ExecutionContext*,
                              const SensorOptions&,
                              ExceptionState&);
  static Magnetometer* Create(ExecutionContext*, ExceptionState&);

  double x(bool& is_null) const;
  double y(bool& is_null) const;
  double z(bool& is_null) const;

  virtual void Trace(blink::Visitor*);

 private:
  Magnetometer(ExecutionContext*, const SensorOptions&, ExceptionState&);
};

}  // namespace blink

#endif  // Magnetometer_h
