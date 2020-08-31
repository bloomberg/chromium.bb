// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_ORIENTATION_SENSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_ORIENTATION_SENSOR_H_

#include "third_party/blink/renderer/bindings/modules/v8/float32_array_or_float64_array_or_dom_matrix.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_spatial_sensor_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/sensor/sensor.h"

namespace blink {

class OrientationSensor : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  base::Optional<Vector<double>> quaternion();
  void populateMatrix(Float32ArrayOrFloat64ArrayOrDOMMatrix&, ExceptionState&);

  bool isReadingDirty() const;

  void Trace(Visitor*) override;

 protected:
  OrientationSensor(ExecutionContext*,
                    const SpatialSensorOptions*,
                    ExceptionState&,
                    device::mojom::blink::SensorType,
                    const Vector<mojom::blink::FeaturePolicyFeature>& features);

 private:
  // SensorProxy override.
  void OnSensorReadingChanged() override;
  template <typename Matrix>
  void PopulateMatrixInternal(Matrix*, ExceptionState&);

  bool reading_dirty_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_ORIENTATION_SENSOR_H_
