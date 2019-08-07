// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_

#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

#include <IOKit/hid/IOHIDManager.h>

namespace device {

class HidHapticGamepadMac : public HidHapticGamepadBase {
 public:
  HidHapticGamepadMac(IOHIDDeviceRef device_ref, const HapticReportData& data);
  ~HidHapticGamepadMac() override;

  static std::unique_ptr<HidHapticGamepadMac> Create(uint16_t vendor_id,
                                                     uint16_t product_id,
                                                     IOHIDDeviceRef device_ref);

  void DoShutdown() override;

  size_t WriteOutputReport(void* report, size_t report_length) override;

 private:
  IOHIDDeviceRef device_ref_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_
