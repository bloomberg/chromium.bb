// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"

#include <cmath>

#include "base/logging.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace device {

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer()
    : PlatformSensorFusionAlgorithm(
          mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES,
          {mojom::SensorType::ACCELEROMETER}) {}

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    ~RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer() =
        default;

bool RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                         SensorReading* fused_reading) {
  // Transform the accelerometer values to W3C draft angles.
  //
  // Accelerometer values are just dot products of the sensor axes
  // by the gravity vector 'g' with the result for the z axis inverted.
  //
  // To understand this transformation calculate the 3rd row of the z-x-y
  // Euler angles rotation matrix (because of the 'g' vector, only 3rd row
  // affects to the result). Note that z-x-y matrix means R = Ry * Rx * Rz.
  // Then, assume alpha = 0 and you get this:
  //
  // x_acc = sin(gamma)
  // y_acc = - cos(gamma) * sin(beta)
  // z_acc = cos(beta) * cos(gamma)
  //
  // After that the rest is just a bit of trigonometry.
  //
  // Also note that alpha can't be provided but it's assumed to be always zero.
  // This is necessary in order to provide enough information to solve
  // the equations.
  //
  DCHECK(fusion_sensor_);

  SensorReading reading;
  if (!fusion_sensor_->GetSourceReading(mojom::SensorType::ACCELEROMETER,
                                        &reading)) {
    return false;
  }

  double acceleration_x = reading.accel.x;
  double acceleration_y = reading.accel.y;
  double acceleration_z = reading.accel.z;

  double alpha = 0.0;
  double beta = std::atan2(-acceleration_y, acceleration_z);
  double gamma = std::asin(acceleration_x / kMeanGravity);

  fused_reading->orientation_euler.x = beta;
  fused_reading->orientation_euler.y = gamma;
  fused_reading->orientation_euler.z = alpha;

  return true;
}

}  // namespace device
