// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AbsoluteOrientationSensor_h
#define AbsoluteOrientationSensor_h

#include "modules/sensor/OrientationSensor.h"

namespace blink {

class AbsoluteOrientationSensor final : public OrientationSensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AbsoluteOrientationSensor* Create(ExecutionContext*,
                                           const SensorOptions&,
                                           ExceptionState&);
  static AbsoluteOrientationSensor* Create(ExecutionContext*, ExceptionState&);

  virtual void Trace(blink::Visitor*);

 private:
  AbsoluteOrientationSensor(ExecutionContext*,
                            const SensorOptions&,
                            ExceptionState&);
};

}  // namespace blink

#endif  // AbsoluteOrientationSensor_h
