// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"

#include <cmath>

#include "base/logging.h"
#include "services/device/generic_sensor/orientation_util.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace {

// Helper function to convert a quaternion to a rotation matrix. x, y, z, w
// are values of a quaternion representing the orientation of the device in
// 3D space. Returns a 9 element rotation matrix:
// r[ 0]   r[ 1]   r[ 2]
// r[ 3]   r[ 4]   r[ 5]
// r[ 6]   r[ 7]   r[ 8]
std::vector<double> ComputeRotationMatrixFromQuaternion(double x,
                                                        double y,
                                                        double z,
                                                        double w) {
  std::vector<double> r(9);

  double sq_x = 2 * x * x;
  double sq_y = 2 * y * y;
  double sq_z = 2 * z * z;
  double x_y = 2 * x * y;
  double z_w = 2 * z * w;
  double x_z = 2 * x * z;
  double y_w = 2 * y * w;
  double y_z = 2 * y * z;
  double x_w = 2 * x * w;

  r[0] = 1 - sq_y - sq_z;
  r[1] = x_y - z_w;
  r[2] = x_z + y_w;
  r[3] = x_y + z_w;
  r[4] = 1 - sq_x - sq_z;
  r[5] = y_z - x_w;
  r[6] = x_z - y_w;
  r[7] = y_z + x_w;
  r[8] = 1 - sq_x - sq_y;

  return r;
}

void ComputeEulerAnglesFromQuaternion(double x,
                                      double y,
                                      double z,
                                      double w,
                                      double* alpha,
                                      double* beta,
                                      double* gamma) {
  std::vector<double> rotation_matrix =
      ComputeRotationMatrixFromQuaternion(x, y, z, w);
  device::ComputeOrientationEulerAnglesFromRotationMatrix(rotation_matrix,
                                                          alpha, beta, gamma);
}

}  // namespace

namespace device {

OrientationEulerAnglesFusionAlgorithmUsingQuaternion::
    OrientationEulerAnglesFusionAlgorithmUsingQuaternion() {}

OrientationEulerAnglesFusionAlgorithmUsingQuaternion::
    ~OrientationEulerAnglesFusionAlgorithmUsingQuaternion() = default;

bool OrientationEulerAnglesFusionAlgorithmUsingQuaternion::GetFusedData(
    mojom::SensorType which_sensor_changed,
    SensorReading* fused_reading) {
  // Transform the *_ORIENTATION_QUATERNION values to
  // *_ORIENTATION_EULER_ANGLES.
  DCHECK(fusion_sensor_);

  SensorReading reading;
  if (!fusion_sensor_->GetLatestReading(0, &reading))
    return false;

  double x = reading.values[0].value();
  double y = reading.values[1].value();
  double z = reading.values[2].value();
  double w = reading.values[3].value();
  double alpha, beta, gamma;
  ComputeEulerAnglesFromQuaternion(x, y, z, w, &alpha, &beta, &gamma);
  fused_reading->values[0].value() = beta;
  fused_reading->values[1].value() = gamma;
  fused_reading->values[2].value() = alpha;
  fused_reading->values[3].value() = 0.0;

  return true;
}

}  // namespace device
