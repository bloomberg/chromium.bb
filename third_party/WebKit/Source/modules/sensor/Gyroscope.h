// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Gyroscope_h
#define Gyroscope_h

#include "modules/sensor/Sensor.h"

namespace blink {

class Gyroscope final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Gyroscope* create(ExecutionContext*,
                           const SensorOptions&,
                           ExceptionState&);
  static Gyroscope* create(ExecutionContext*, ExceptionState&);

  double x(bool& isNull) const;
  double y(bool& isNull) const;
  double z(bool& isNull) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Gyroscope(ExecutionContext*, const SensorOptions&, ExceptionState&);
};

}  // namespace blink

#endif  // Gyroscope_h
