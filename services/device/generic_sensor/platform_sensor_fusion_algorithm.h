// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

class PlatformSensorFusion;

// Base class for platform sensor fusion algorithm.
class PlatformSensorFusionAlgorithm {
 public:
  PlatformSensorFusionAlgorithm();
  virtual ~PlatformSensorFusionAlgorithm();

  void set_threshold(double threshold) { threshold_ = threshold; }

  void set_fusion_sensor(PlatformSensorFusion* fusion_sensor) {
    fusion_sensor_ = fusion_sensor;
  }

  bool IsReadingSignificantlyDifferent(const SensorReading& reading1,
                                       const SensorReading& reading2);

  virtual bool GetFusedData(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) = 0;

  // Sets frequency at which data is expected to be obtained from the platform.
  // This might be needed e.g., to calculate cut-off frequency of low/high-pass
  // filters.
  virtual void SetFrequency(double frequency);

  // Algorithms that use statistical data (moving average, Kalman filter, etc),
  // might need to be reset when sensor is stopped.
  virtual void Reset();

 protected:
  // This raw pointer is safe because |fusion_sensor_| owns this object.
  PlatformSensorFusion* fusion_sensor_ = nullptr;

 private:
  // Default threshold for comparing SensorReading values. If a
  // different threshold is better for a certain sensor type, set_threshold()
  // should be used to change it.
  double threshold_ = 0.1;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorFusionAlgorithm);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_
