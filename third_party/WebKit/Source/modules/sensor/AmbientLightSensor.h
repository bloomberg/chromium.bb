// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AmbientLightSensor_h
#define AmbientLightSensor_h

#include "modules/sensor/Sensor.h"

namespace blink {

class AmbientLightSensor final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AmbientLightSensor* Create(ExecutionContext*,
                                    const SensorOptions&,
                                    ExceptionState&);
  static AmbientLightSensor* Create(ExecutionContext*, ExceptionState&);

  double illuminance(bool& is_null) const;

  virtual void Trace(blink::Visitor*);

 private:
  AmbientLightSensor(ExecutionContext*, const SensorOptions&, ExceptionState&);
};

}  // namespace blink

#endif  // AmbientLightSensor_h
