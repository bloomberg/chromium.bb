// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

using mojom::SensorType;

double GetSensorMaxAllowedFrequency(SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return SensorTraits<SensorType::AMBIENT_LIGHT>::kMaxAllowedFrequency;
    case SensorType::PROXIMITY:
      return SensorTraits<SensorType::PROXIMITY>::kMaxAllowedFrequency;
    case SensorType::ACCELEROMETER:
      return SensorTraits<SensorType::ACCELEROMETER>::kMaxAllowedFrequency;
    case SensorType::LINEAR_ACCELERATION:
      return SensorTraits<
          SensorType::LINEAR_ACCELERATION>::kMaxAllowedFrequency;
    case SensorType::GYROSCOPE:
      return SensorTraits<SensorType::GYROSCOPE>::kMaxAllowedFrequency;
    case SensorType::MAGNETOMETER:
      return SensorTraits<SensorType::MAGNETOMETER>::kMaxAllowedFrequency;
    case SensorType::PRESSURE:
      return SensorTraits<SensorType::PRESSURE>::kMaxAllowedFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES>::kMaxAllowedFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_QUATERNION>::kMaxAllowedFrequency;
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_EULER_ANGLES>::kMaxAllowedFrequency;
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_QUATERNION>::kMaxAllowedFrequency;
    default:
      NOTREACHED() << "Unknown sensor type " << type;
      return SensorTraits<SensorType::LAST>::kMaxAllowedFrequency;
  }
}

}  // namespace device
