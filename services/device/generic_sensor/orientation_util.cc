// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "services/device/generic_sensor/orientation_util.h"

#include <cmath>

#include "base/logging.h"

namespace {

// Returns orientation angles from a rotation matrix, such that the angles are
// according to spec http://dev.w3.org/geo/api/spec-source-orientation.html}.
//
// It is assumed the rotation matrix transforms a 3D column vector from device
// coordinate system to the world's coordinate system.
//
// In particular we compute the decomposition of a given rotation matrix r such
// that
// r = rz(alpha) * rx(beta) * ry(gamma)
// where rz, rx and ry are rotation matrices around z, x and y axes in the world
// coordinate reference frame respectively. The reference frame consists of
// three orthogonal axes x, y, z where x points East, y points north and z
// points upwards perpendicular to the ground plane. The computed angles alpha,
// beta and gamma are in degrees and clockwise-positive when viewed along the
// positive direction of the corresponding axis. Except for the special case
// when the beta angle is +-pi/2 these angles uniquely define the orientation of
// a mobile device in 3D space. The alpha-beta-gamma representation resembles
// the yaw-pitch-roll convention used in vehicle dynamics, however it does not
// exactly match it. One of the differences is that the 'pitch' angle beta is
// allowed to be within [-pi, pi). A mobile device with pitch angle greater than
// pi/2 could correspond to a user lying down and looking upward at the screen.
//
// r is a 9 element rotation matrix:
// r[ 0]   r[ 1]   r[ 2]
// r[ 3]   r[ 4]   r[ 5]
// r[ 6]   r[ 7]   r[ 8]
//
// alpha: rotation around the z axis, in [0, 2*pi)
// beta: rotation around the x axis, in [-pi, pi)
// gamma: rotation around the y axis, in [-pi/2, pi/2)
void ComputeOrientationEulerAnglesInRadiansFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha,
    double* beta,
    double* gamma) {
  DCHECK_EQ(9u, r.size());

  if (r[8] > 0) {  // cos(beta) > 0
    *alpha = std::atan2(-r[1], r[4]);
    *beta = std::asin(r[7]);           // beta (-pi/2, pi/2)
    *gamma = std::atan2(-r[6], r[8]);  // gamma (-pi/2, pi/2)
  } else if (r[8] < 0) {               // cos(beta) < 0
    *alpha = std::atan2(r[1], -r[4]);
    *beta = -std::asin(r[7]);
    *beta += (*beta >= 0) ? -M_PI : M_PI;  // beta [-pi,-pi/2) U (pi/2,pi)
    *gamma = std::atan2(r[6], -r[8]);      // gamma (-pi/2, pi/2)
  } else {                                 // r[8] == 0
    if (r[6] > 0) {                        // cos(gamma) == 0, cos(beta) > 0
      *alpha = std::atan2(-r[1], r[4]);
      *beta = std::asin(r[7]);  // beta [-pi/2, pi/2]
      *gamma = -M_PI / 2;       // gamma = -pi/2
    } else if (r[6] < 0) {      // cos(gamma) == 0, cos(beta) < 0
      *alpha = std::atan2(r[1], -r[4]);
      *beta = -std::asin(r[7]);
      *beta += (*beta >= 0) ? -M_PI : M_PI;  // beta [-pi,-pi/2) U (pi/2,pi)
      *gamma = -M_PI / 2;                    // gamma = -pi/2
    } else {                                 // r[6] == 0, cos(beta) == 0
      // gimbal lock discontinuity
      *alpha = std::atan2(r[3], r[0]);
      *beta = (r[7] > 0) ? M_PI / 2 : -M_PI / 2;  // beta = +-pi/2
      *gamma = 0;                                 // gamma = 0
    }
  }

  // alpha is in [-pi, pi], make sure it is in [0, 2*pi).
  if (*alpha < 0)
    *alpha += 2 * M_PI;  // alpha [0, 2*pi)
}

double RadiansToDegrees(double radians) {
  return (180.0 * radians) / M_PI;
}

}  // namespace

namespace device {

void ComputeOrientationEulerAnglesFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha,
    double* beta,
    double* gamma) {
  double alpha_radians, beta_radians, gamma_radians;
  ComputeOrientationEulerAnglesInRadiansFromRotationMatrix(
      r, &alpha_radians, &beta_radians, &gamma_radians);
  *alpha = RadiansToDegrees(alpha_radians);
  *beta = RadiansToDegrees(beta_radians);
  *gamma = RadiansToDegrees(gamma_radians);
}

}  // namespace device
