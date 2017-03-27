// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationSensor_h
#define OrientationSensor_h

#include "core/dom/DOMTypedArray.h"
#include "modules/sensor/Sensor.h"

namespace blink {

class OrientationSensor : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Vector<double> quaternion(bool& isNull);
  void populateMatrix(DOMFloat32Array*, ExceptionState&);

  bool isReadingDirty() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  OrientationSensor(ExecutionContext*,
                    const SensorOptions&,
                    ExceptionState&,
                    device::mojom::blink::SensorType);

 private:
  // SensorProxy override.
  void onSensorReadingChanged() override;

  bool m_readingDirty;
};

}  // namespace blink

#endif  // OrientationSensor_h
