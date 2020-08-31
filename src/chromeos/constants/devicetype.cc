// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/devicetype.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_split.h"
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

bool IsGoogleBrandedDevice() {
  // Refer to comment describing base::SysInfo::GetLsbReleaseBoard for why
  // splitting the Lsb Release Board string is needed.
  std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board.empty())
    return false;

  // TODO(crbug/966108): This method of determining board names will leak
  // hardware codenames.  Unreleased boards should NOT be included in this list.
  return board[0] == "nocturne" || board[0] == "eve" || board[0] == "atlas" ||
         board[0] == "samus";
}

}  // namespace chromeos
