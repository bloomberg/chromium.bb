// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_TEST_REPORT_DESCRIPTORS_H_
#define SERVICES_DEVICE_HID_TEST_REPORT_DESCRIPTORS_H_

#include <stddef.h>
#include <stdint.h>

namespace device {

// Digitizer descriptor from HID descriptor tool
// http://www.usb.org/developers/hidpage/dt2_4.zip
extern const uint8_t kDigitizer[];
extern const size_t kDigitizerSize;

// Keyboard descriptor from HID descriptor tool
// http://www.usb.org/developers/hidpage/dt2_4.zip
extern const uint8_t kKeyboard[];
extern const size_t kKeyboardSize;

// Monitor descriptor from HID descriptor tool
// http://www.usb.org/developers/hidpage/dt2_4.zip
extern const uint8_t kMonitor[];
extern const size_t kMonitorSize;

// Mouse descriptor from HID descriptor tool
// http://www.usb.org/developers/hidpage/dt2_4.zip
extern const uint8_t kMouse[];
extern const size_t kMouseSize;

// Logitech Unifying receiver descriptor
extern const uint8_t kLogitechUnifyingReceiver[];
extern const size_t kLogitechUnifyingReceiverSize;

// Sony Dualshock 3 USB descriptor
extern const uint8_t kSonyDualshock3[];
extern const size_t kSonyDualshock3Size;

// Sony Dualshock 4 USB descriptor
extern const uint8_t kSonyDualshock4[];
extern const size_t kSonyDualshock4Size;

// Microsoft Xbox Wireless Controller Bluetooth descriptor
extern const uint8_t kMicrosoftXboxWirelessController[];
extern const size_t kMicrosoftXboxWirelessControllerSize;

// Nintendo Switch Pro Controller USB descriptor
extern const uint8_t kNintendoSwitchProController[];
extern const size_t kNintendoSwitchProControllerSize;

// Microsoft Xbox Adaptive Controller Bluetooth descriptor
extern const uint8_t kMicrosoftXboxAdaptiveController[];
extern const size_t kMicrosoftXboxAdaptiveControllerSize;

// Nexus Player Controller descriptor
extern const uint8_t kNexusPlayerController[];
extern const size_t kNexusPlayerControllerSize;

// Steam Controller descriptors
extern const uint8_t kSteamControllerKeyboard[];
extern const size_t kSteamControllerKeyboardSize;
extern const uint8_t kSteamControllerMouse[];
extern const size_t kSteamControllerMouseSize;
extern const uint8_t kSteamControllerVendor[];
extern const size_t kSteamControllerVendorSize;

// XSkills Gamecube USB controller adapter descriptor
extern const uint8_t kXSkillsUsbAdapter[];
extern const size_t kXSkillsUsbAdapterSize;

// Belkin Nostromo SpeedPad descriptors
extern const uint8_t kBelkinNostromoKeyboard[];
extern const size_t kBelkinNostromoKeyboardSize;
extern const uint8_t kBelkinNostromoMouseAndExtra[];
extern const size_t kBelkinNostromoMouseAndExtraSize;

}  // namespace device

#endif  // SERVICES_DEVICE_HID_TEST_REPORT_DESCRIPTORS_H_
