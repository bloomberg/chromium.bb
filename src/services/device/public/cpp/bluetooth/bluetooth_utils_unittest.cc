// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

using mojom::BluetoothDeviceInfo;
using mojom::BluetoothDeviceInfoPtr;

constexpr std::array<uint8_t, 6> kAddress = {0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00};
constexpr char kName[] = "Foo Bar";
constexpr char kUnicodeName[] = "❤❤❤❤";
constexpr char kEmptyName[] = "";
constexpr char kWhitespaceName[] = "    ";
constexpr char kUnicodeWhitespaceName[] = "　　　　";

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_NoNameAndNoUnknownDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = base::nullopt;
  info->device_type = BluetoothDeviceInfo::DeviceType::kUnknown;
  EXPECT_EQ(
      base::UTF8ToUTF16("Unknown or Unsupported Device (00:00:00:00:00:00)"),
      GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_NoNameAndPeripheralDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = base::nullopt;
  info->device_type = BluetoothDeviceInfo::DeviceType::kPeripheral;
  EXPECT_EQ(base::UTF8ToUTF16("Peripheral (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_NoNameAndComputerDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = base::nullopt;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_NameAndUnknownDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kUnknown;
  EXPECT_EQ(base::UTF8ToUTF16(kName), GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_NameAndComputerDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16(kName), GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, GetBluetoothDeviceNameForDisplay_UnicodeName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kUnicodeName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16(kUnicodeName),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, GetBluetoothDeviceNameForDisplay_EmptyName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kEmptyName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, GetBluetoothDeviceNameForDisplay_WhitespaceName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kWhitespaceName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest,
     GetBluetoothDeviceNameForDisplay_UnicodeWhitespaceName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kUnicodeWhitespaceName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, GetBluetoothAddressForDisplay) {
  EXPECT_EQ(
      base::UTF8ToUTF16("AA:BB:CC:00:11:22"),
      GetBluetoothAddressForDisplay({0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x22}));
}

static base::string16 LabelFromTypeWithName(
    BluetoothDeviceInfo::DeviceType type,
    const char* name = kName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = name;
  info->device_type = type;
  return GetBluetoothDeviceLabelForAccessibility(info);
}

TEST(BluetoothUtilsTest, GetBluetoothDeviceLabelForAccessibility) {
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_COMPUTER,
          base::UTF8ToUTF16(kName)),
      LabelFromTypeWithName(BluetoothDeviceInfo::DeviceType::kComputer, kName));

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_CAR_AUDIO,
                base::UTF8ToUTF16(kUnicodeName)),
            LabelFromTypeWithName(BluetoothDeviceInfo::DeviceType::kCarAudio,
                                  kUnicodeName));
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD,
                base::UTF8ToUTF16("00:00:00:00:00:00")),
            LabelFromTypeWithName(BluetoothDeviceInfo::DeviceType::kKeyboard,
                                  kEmptyName));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_VIDEO,
                                 base::UTF8ToUTF16("00:00:00:00:00:00")),
      LabelFromTypeWithName(BluetoothDeviceInfo::DeviceType::kVideo,
                            kWhitespaceName));
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_JOYSTICK,
                base::UTF8ToUTF16("00:00:00:00:00:00")),
            LabelFromTypeWithName(BluetoothDeviceInfo::DeviceType::kJoystick,
                                  kUnicodeWhitespaceName));
}

}  // namespace device
