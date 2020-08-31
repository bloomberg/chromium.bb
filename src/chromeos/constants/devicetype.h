// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_DEVICETYPE_H_
#define CHROMEOS_CONSTANTS_DEVICETYPE_H_

#include "base/component_export.h"

namespace chromeos {

enum class DeviceType {
  kChromebase,
  kChromebit,
  kChromebook,
  kChromebox,
  kUnknown,  // Unknown fallback device.
};

// Returns the current device type, eg, Chromebook, Chromebox.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) DeviceType GetDeviceType();

// Returns true if the device is Google branded.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsGoogleBrandedDevice();

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_DEVICETYPE_H_
