// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelativeOrientationSensor_h
#define RelativeOrientationSensor_h

#include "modules/sensor/OrientationSensor.h"

namespace blink {

class RelativeOrientationSensor final : public OrientationSensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RelativeOrientationSensor* Create(ExecutionContext*,
                                           const SensorOptions&,
                                           ExceptionState&);
  static RelativeOrientationSensor* Create(ExecutionContext*, ExceptionState&);

  virtual void Trace(blink::Visitor*);

 private:
  RelativeOrientationSensor(ExecutionContext*,
                            const SensorOptions&,
                            ExceptionState&);
};

}  // namespace blink

#endif  // RelativeOrientationSensor_h
