// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelativeOrientationSensor_h
#define RelativeOrientationSensor_h

#include "modules/sensor/OrientationSensor.h"
#include "modules/sensor/SpatialSensorOptions.h"

namespace blink {

class RelativeOrientationSensor final : public OrientationSensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RelativeOrientationSensor* Create(ExecutionContext*,
                                           const SpatialSensorOptions&,
                                           ExceptionState&);
  static RelativeOrientationSensor* Create(ExecutionContext*, ExceptionState&);

  virtual void Trace(blink::Visitor*);

 private:
  RelativeOrientationSensor(ExecutionContext*,
                            const SpatialSensorOptions&,
                            ExceptionState&);
};

}  // namespace blink

#endif  // RelativeOrientationSensor_h
