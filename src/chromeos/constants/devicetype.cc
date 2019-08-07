// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/devicetype.h"

#include <string>

#include "base/system/sys_info.h"

namespace chromeos {

namespace {
const char kDeviceTypeKey[] = "DEVICETYPE";
}

DeviceType GetDeviceType() {
  std::string value;
  if (base::SysInfo::GetLsbReleaseValue(kDeviceTypeKey, &value)) {
    if (value == "CHROMEBASE")
      return DeviceType::kChromebase;
    if (value == "CHROMEBIT")
      return DeviceType::kChromebit;
    // Most devices are Chromebooks, so we will also consider reference boards
    // as chromebooks.
    if (value == "CHROMEBOOK" || value == "REFERENCE")
      return DeviceType::kChromebook;
    if (value == "CHROMEBOX")
      return DeviceType::kChromebox;
    LOG(ERROR) << "Unknown device type \"" << value << "\"";
  }

  return DeviceType::kUnknown;
}

}  // namespace chromeos
