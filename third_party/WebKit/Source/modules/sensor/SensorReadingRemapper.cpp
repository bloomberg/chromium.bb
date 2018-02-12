// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReadingRemapper.h"

using device::SensorReading;
using device::SensorReadingXYZ;
using device::mojom::blink::SensorType;

namespace blink {

namespace {
constexpr int ScreenAngleSin(uint16_t angle) {
  switch (angle) {
    case 0:
      return 0;
    case 90:
      return 1;
    case 180:
      return 0;
    case 270:
      return -1;
    default:
      NOTREACHED();
      return 0;
  }
}

constexpr int ScreenAngleCos(uint16_t angle) {
  switch (angle) {
    case 0:
      return 1;
    case 90:
      return 0;
    case 180:
      return -1;
    case 270:
      return 0;
    default:
      NOTREACHED();
      return 1;
  }
}

void RemapSensorReading(uint16_t angle, SensorReadingXYZ& reading) {
  int cos = ScreenAngleCos(angle);
  int sin = ScreenAngleSin(angle);
  double x = reading.x;
  double y = reading.y;

  reading.x = x * cos + y * sin;
  reading.y = y * cos - x * sin;
}

}  // namespace

// static
void SensorReadingRemapper::RemapToScreenCoords(
    SensorType type,
    uint16_t angle,
    device::SensorReading* reading) {
  DCHECK(reading);
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
    case SensorType::PROXIMITY:
    case SensorType::PRESSURE:
      NOTREACHED() << "Remap must not be performed for the sensor type "
                   << type;
      break;
    case SensorType::ACCELEROMETER:
    case SensorType::LINEAR_ACCELERATION:
      RemapSensorReading(angle, reading->accel);
      break;
    case SensorType::GYROSCOPE:
      RemapSensorReading(angle, reading->gyro);
      break;
    case SensorType::MAGNETOMETER:
      RemapSensorReading(angle, reading->magn);
      break;
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      NOTREACHED() << "Remap is not yet implemented for the sensor type "
                   << type;
      break;
    default:
      NOTREACHED() << "Unknown sensor type " << type;
  }
}

}  // namespace blink
