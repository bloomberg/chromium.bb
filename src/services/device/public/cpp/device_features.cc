// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
const base::Feature kGenericSensorExtraClasses{
    "GenericSensorExtraClasses", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the Windows.Devices.Geolocation WinRT API for the
// LocationProvider instead of the NetworkLocationProvider on Windows.
const base::Feature kWinrtGeolocationImplementation{
    "WinrtGeolocationImplementation", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the CoreLocation API for LocationProvider instead of
// NetworkLocationProvider for macOS.
const base::Feature kMacCoreLocationBackend{"MacCoreLocationBackend",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
// Controls whether Web Bluetooth should request for a larger ATT MTU so that
// more information can be exchanged per transmission.
const base::Feature kWebBluetoothRequestLargerMtu{
    "WebBluetoothRequestLargerMtu", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
