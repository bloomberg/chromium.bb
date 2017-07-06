// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinearAccelerationSensor_h
#define LinearAccelerationSensor_h

#include "modules/sensor/Accelerometer.h"

namespace blink {

class LinearAccelerationSensor final : public Accelerometer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static LinearAccelerationSensor* Create(ExecutionContext*,
                                          const SensorOptions&,
                                          ExceptionState&);
  static LinearAccelerationSensor* Create(ExecutionContext*, ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

 private:
  LinearAccelerationSensor(ExecutionContext*,
                           const SensorOptions&,
                           ExceptionState&);
};

}  // namespace blink

#endif  // LinearAccelerationSensor_h
