// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_SWITCHES_MODEMFWD_SWITCHES_H_
#define SYSTEM_API_SWITCHES_MODEMFWD_SWITCHES_H_

// This file defines switches that are used by modemfwd.
// See its README.md file for more information about what each one does.

namespace modemfwd {

const char kGetFirmwareInfo[] = "get_fw_info";
const char kPrepareToFlash[] = "prepare_to_flash";
const char kFlashMainFirmware[] = "flash_main_fw";
const char kFlashCarrierFirmware[] = "flash_carrier_fw";
const char kFlashModeCheck[] = "flash_mode_check";
const char kReboot[] = "reboot";

const char kFwVersion[] = "fw_version";

}  // namespace modemfwd

#endif  // SYSTEM_API_SWITCHES_MODEMFWD_SWITCHES_H_
