// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

#include <string>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/string_util_icu.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

using DeviceType = mojom::BluetoothDeviceInfo::DeviceType;

base::string16 GetBluetoothAddressForDisplay(
    const std::array<uint8_t, 6>& address) {
  static constexpr char kAddressFormat[] =
      "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX";

  return base::UTF8ToUTF16(
      base::StringPrintf(kAddressFormat, address[0], address[1], address[2],
                         address[3], address[4], address[5]));
}

base::string16 GetBluetoothDeviceNameForDisplay(
    const mojom::BluetoothDeviceInfoPtr& device_info) {
  if (device_info->name) {
    const std::string& device_name = device_info->name.value();
    if (HasGraphicCharacter(device_name))
      return base::UTF8ToUTF16(device_name);
  }

  auto address_utf16 = GetBluetoothAddressForDisplay(device_info->address);
  auto device_type = device_info->device_type;
  switch (device_type) {
    case DeviceType::kUnknown:
    case DeviceType::kPeripheral:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
    case DeviceType::kComputer:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case DeviceType::kPhone:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case DeviceType::kModem:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case DeviceType::kAudio:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case DeviceType::kCarAudio:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case DeviceType::kVideo:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case DeviceType::kJoystick:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case DeviceType::kGamepad:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case DeviceType::kKeyboard:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case DeviceType::kMouse:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case DeviceType::kTablet:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case DeviceType::kKeyboardMouseCombo:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
  }

  NOTREACHED();
}

}  // namespace device
