// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/accelerometer/accelerometer_util.h"

#include <cmath>

#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {

// The maximum deviation from the acceleration expected due to gravity for which
// the device will be considered stable: 1g.
const float kDeviationFromGravityThreshold = 1.0f;

// The mean acceleration due to gravity on Earth in m/s^2.
const float kMeanGravity = 9.80665f;

}  // namespace

namespace ui {

const gfx::Vector3dF ConvertAccelerometerReadingToVector3dF(
    const chromeos::AccelerometerReading& reading) {
  return gfx::Vector3dF(reading.x, reading.y, reading.z);
}

bool IsAccelerometerReadingStable(const chromeos::AccelerometerUpdate& update,
                                  chromeos::AccelerometerSource source) {
  return update.has(source) &&
         std::abs(ConvertAccelerometerReadingToVector3dF(update.get(source))
                      .Length() -
                  kMeanGravity) <= kDeviationFromGravityThreshold;
}

}  // namespace ui
