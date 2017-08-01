// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

#include <cmath>

namespace device {

PlatformSensorFusionAlgorithm::PlatformSensorFusionAlgorithm() {}

PlatformSensorFusionAlgorithm::~PlatformSensorFusionAlgorithm() = default;

bool PlatformSensorFusionAlgorithm::IsReadingSignificantlyDifferent(
    const SensorReading& reading1,
    const SensorReading& reading2) {
  return (std::fabs(reading1.values[0] - reading2.values[0]) >= threshold_) ||
         (std::fabs(reading1.values[1] - reading2.values[1]) >= threshold_) ||
         (std::fabs(reading1.values[2] - reading2.values[2]) >= threshold_) ||
         (std::fabs(reading1.values[3] - reading2.values[3]) >= threshold_);
}

void PlatformSensorFusionAlgorithm::Reset() {}

void PlatformSensorFusionAlgorithm::SetFrequency(double) {}

}  // namespace device
