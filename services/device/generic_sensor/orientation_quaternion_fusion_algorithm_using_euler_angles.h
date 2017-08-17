// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_

#include "base/macros.h"
#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Sensor fusion algorithm for implementing *_ORIENTATION_QUATERNION
// using *_ORIENTATION_EULER_ANGLES.
class OrientationQuaternionFusionAlgorithmUsingEulerAngles
    : public PlatformSensorFusionAlgorithm {
 public:
  explicit OrientationQuaternionFusionAlgorithmUsingEulerAngles(bool absolute);
  ~OrientationQuaternionFusionAlgorithmUsingEulerAngles() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      OrientationQuaternionFusionAlgorithmUsingEulerAngles);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_
