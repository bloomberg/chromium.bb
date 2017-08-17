// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"

#include <cmath>

#include "base/logging.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace device {

namespace {

void ComputeQuaternionFromEulerAngles(double alpha,
                                      double beta,
                                      double gamma,
                                      double* x,
                                      double* y,
                                      double* z,
                                      double* w) {
  double cx = std::cos(beta / 2);
  double cy = std::cos(gamma / 2);
  double cz = std::cos(alpha / 2);
  double sx = std::sin(beta / 2);
  double sy = std::sin(gamma / 2);
  double sz = std::sin(alpha / 2);

  *x = sx * cy * cz - cx * sy * sz;
  *y = cx * sy * cz + sx * cy * sz;
  *z = cx * cy * sz + sx * sy * cz;
  *w = cx * cy * cz - sx * sy * sz;
}

constexpr mojom::SensorType GetFusedType(bool absolute) {
  return absolute ? mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION
                  : mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION;
}

constexpr mojom::SensorType GetSourceType(bool absolute) {
  return absolute ? mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES
                  : mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES;
}

}  // namespace

OrientationQuaternionFusionAlgorithmUsingEulerAngles::
    OrientationQuaternionFusionAlgorithmUsingEulerAngles(bool absolute)
    : PlatformSensorFusionAlgorithm(GetFusedType(absolute),
                                    {GetSourceType(absolute)}) {}

OrientationQuaternionFusionAlgorithmUsingEulerAngles::
    ~OrientationQuaternionFusionAlgorithmUsingEulerAngles() = default;

bool OrientationQuaternionFusionAlgorithmUsingEulerAngles::GetFusedDataInternal(
    mojom::SensorType which_sensor_changed,
    SensorReading* fused_reading) {
  // Transform the *_ORIENTATION_EULER_ANGLES values to
  // *_ORIENTATION_QUATERNION.
  DCHECK(fusion_sensor_);

  SensorReading reading;
  if (!fusion_sensor_->GetSourceReading(which_sensor_changed, &reading))
    return false;

  double beta = reading.orientation_euler.x;
  double gamma = reading.orientation_euler.y;
  double alpha = reading.orientation_euler.z;
  double x, y, z, w;
  ComputeQuaternionFromEulerAngles(alpha, beta, gamma, &x, &y, &z, &w);
  fused_reading->orientation_quat.x = x;
  fused_reading->orientation_quat.y = y;
  fused_reading->orientation_quat.z = z;
  fused_reading->orientation_quat.w = w;

  return true;
}

}  // namespace device
