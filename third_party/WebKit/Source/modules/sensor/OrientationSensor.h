// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationSensor_h
#define OrientationSensor_h

#include "bindings/modules/v8/Float32ArrayOrFloat64ArrayOrDOMMatrix.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/sensor/Sensor.h"

namespace blink {

class OrientationSensor : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Vector<double> quaternion(bool& is_null);
  void populateMatrix(Float32ArrayOrFloat64ArrayOrDOMMatrix&, ExceptionState&);

  bool isReadingDirty() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  OrientationSensor(ExecutionContext*,
                    const SensorOptions&,
                    ExceptionState&,
                    device::mojom::blink::SensorType);

 private:
  // SensorProxy override.
  void OnSensorReadingChanged() override;
  template <typename Matrix>
  void PopulateMatrixInternal(Matrix*, ExceptionState&);

  bool reading_dirty_;
};

}  // namespace blink

#endif  // OrientationSensor_h
