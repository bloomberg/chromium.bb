// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Accelerometer_h
#define Accelerometer_h

#include "modules/sensor/AccelerometerOptions.h"
#include "modules/sensor/Sensor.h"

namespace blink {

class Accelerometer final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Accelerometer* create(ExecutionContext*,
                               const AccelerometerOptions&,
                               ExceptionState&);
  static Accelerometer* create(ExecutionContext*, ExceptionState&);

  double x(bool& isNull) const;
  double y(bool& isNull) const;
  double z(bool& isNull) const;
  bool includesGravity() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Accelerometer(ExecutionContext*,
                const AccelerometerOptions&,
                ExceptionState&);
  // Sensor overrides.
  AccelerometerOptions m_accelerometerOptions;
};

}  // namespace blink

#endif  // Accelerometer_h
