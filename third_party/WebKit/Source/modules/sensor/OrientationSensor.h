// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationSensor_h
#define OrientationSensor_h

#include "bindings/modules/v8/float32_array_or_float64_array_or_dom_matrix.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/sensor/Sensor.h"

namespace blink {

class OrientationSensor : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Vector<double> quaternion(bool& is_null);
  void populateMatrix(Float32ArrayOrFloat64ArrayOrDOMMatrix&, ExceptionState&);

  bool isReadingDirty() const;

  virtual void Trace(blink::Visitor*);

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
