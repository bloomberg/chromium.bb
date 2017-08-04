// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_

#include <vector>

namespace device {

void ComputeOrientationEulerAnglesFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha,
    double* beta,
    double* gamma);

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_
