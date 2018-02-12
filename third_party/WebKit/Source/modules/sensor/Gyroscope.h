// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Gyroscope_h
#define Gyroscope_h

#include "modules/sensor/Sensor.h"
#include "modules/sensor/SpatialSensorOptions.h"

namespace blink {

class Gyroscope final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Gyroscope* Create(ExecutionContext*,
                           const SpatialSensorOptions&,
                           ExceptionState&);
  static Gyroscope* Create(ExecutionContext*, ExceptionState&);

  double x(bool& is_null) const;
  double y(bool& is_null) const;
  double z(bool& is_null) const;

  virtual void Trace(blink::Visitor*);

 private:
  Gyroscope(ExecutionContext*, const SpatialSensorOptions&, ExceptionState&);
};

}  // namespace blink

#endif  // Gyroscope_h
