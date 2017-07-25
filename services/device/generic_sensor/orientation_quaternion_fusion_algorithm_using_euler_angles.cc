// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"

#include <cmath>

#include "services/device/generic_sensor/generic_sensor_consts.h"

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

}  // namespace

namespace device {

OrientationQuaternionFusionAlgorithmUsingEulerAngles::
    OrientationQuaternionFusionAlgorithmUsingEulerAngles() {}

OrientationQuaternionFusionAlgorithmUsingEulerAngles::
    ~OrientationQuaternionFusionAlgorithmUsingEulerAngles() = default;

void OrientationQuaternionFusionAlgorithmUsingEulerAngles::GetFusedData(
    const std::vector<SensorReading>& readings,
    SensorReading* fused_reading) {
  // Transform the *_ORIENTATION_EULER_ANGLES values to
  // *_ORIENTATION_QUATERNION.
  DCHECK(readings.size() == 1);

  double beta = readings[0].values[0].value();
  double gamma = readings[0].values[1].value();
  double alpha = readings[0].values[2].value();
  double x, y, z, w;
  ComputeQuaternionFromEulerAngles(alpha, beta, gamma, &x, &y, &z, &w);
  fused_reading->values[0].value() = x;
  fused_reading->values[1].value() = y;
  fused_reading->values[2].value() = z;
  fused_reading->values[3].value() = w;
}

}  // namespace device
