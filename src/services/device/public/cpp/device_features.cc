// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables sensors based on Generic Sensor API:
// https://w3c.github.io/sensors/
const base::Feature kGenericSensor{"GenericSensor",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
const base::Feature kGenericSensorExtraClasses{
    "GenericSensorExtraClasses", base::FEATURE_DISABLED_BY_DEFAULT};
// Enable UI in the content settings to control access to the sensor APIs
// (Generic Sensor and Device Orientation).
const base::Feature kSensorContentSetting{"SensorContentSetting",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
// Enables usage of the Windows.Devices.Sensors WinRT API for the sensor
// backend instead of the ISensor API on Windows.
const base::Feature kWinrtSensorsImplementation{
    "WinrtSensorsImplementation", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
