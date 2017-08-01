// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"

namespace device {

LinearAccelerationFusionAlgorithmUsingAccelerometer::
    LinearAccelerationFusionAlgorithmUsingAccelerometer() {
  Reset();
}

LinearAccelerationFusionAlgorithmUsingAccelerometer::
    ~LinearAccelerationFusionAlgorithmUsingAccelerometer() = default;

void LinearAccelerationFusionAlgorithmUsingAccelerometer::SetFrequency(
    double frequency) {
  DCHECK(frequency > 0);
  // Set time constant to be bound to requested rate, if sensor can provide
  // data at such rate, low-pass filter alpha would be ~0.5 that is effective
  // at smoothing data and provides minimal latency from LPF.
  time_constant_ = 1 / frequency;
}

void LinearAccelerationFusionAlgorithmUsingAccelerometer::Reset() {
  reading_updates_count_ = 0;
  time_constant_ = 0.0;
  initial_timestamp_ = 0.0;
  gravity_x_ = 0.0;
  gravity_y_ = 0.0;
  gravity_z_ = 0.0;
}

void LinearAccelerationFusionAlgorithmUsingAccelerometer::GetFusedData(
    const std::vector<SensorReading>& readings,
    SensorReading* fused_reading) {
  DCHECK(readings.size() == 1);

  ++reading_updates_count_;

  // First reading.
  if (initial_timestamp_ == 0.0) {
    initial_timestamp_ = readings[0].timestamp;
    return;
  }

  double delivery_rate =
      (readings[0].timestamp - initial_timestamp_) / reading_updates_count_;
  double alpha = time_constant_ / (time_constant_ + delivery_rate);

  double acceleration_x = readings[0].values[0].value();
  double acceleration_y = readings[0].values[1].value();
  double acceleration_z = readings[0].values[2].value();

  // Isolate gravity.
  gravity_x_ = alpha * gravity_x_ + (1 - alpha) * acceleration_x;
  gravity_y_ = alpha * gravity_y_ + (1 - alpha) * acceleration_y;
  gravity_z_ = alpha * gravity_z_ + (1 - alpha) * acceleration_z;

  // Get linear acceleration.
  fused_reading->values[0].value() = acceleration_x - gravity_x_;
  fused_reading->values[1].value() = acceleration_y - gravity_y_;
  fused_reading->values[2].value() = acceleration_z - gravity_z_;
}

}  // namespace device
