// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using mojom::BluetoothDeviceInfo;
using mojom::BluetoothDeviceInfoPtr;

constexpr char kAddress[] = "00:00:00:00:00:00";
constexpr char kName[] = "Foo Bar";
constexpr char kUnicodeName[] = "❤❤❤❤";
constexpr char kEmptyName[] = "";
constexpr char kWhitespaceName[] = "    ";
constexpr char kUnicodeWhitespaceName[] = "　　　　";

TEST(BluetoothUtilsTest, NoNameAndNoUnknownDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = base::nullopt;
  info->device_type = BluetoothDeviceInfo::DeviceType::kUnknown;
  EXPECT_EQ(
      base::UTF8ToUTF16("Unknown or Unsupported Device (00:00:00:00:00:00)"),
      GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, NoNameAndComputerDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = base::nullopt;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, NameAndUnknownDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kUnknown;
  EXPECT_EQ(base::UTF8ToUTF16(kName), GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTest, NameAndComputerDeviceType) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16(kName), GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTests, UnicodeName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kUnicodeName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16(kUnicodeName),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTests, EmptyName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kEmptyName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTests, WhitespaceName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kWhitespaceName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

TEST(BluetoothUtilsTests, UnicodeWhitespaceName) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = kAddress;
  info->name = kUnicodeWhitespaceName;
  info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
  EXPECT_EQ(base::UTF8ToUTF16("Computer (00:00:00:00:00:00)"),
            GetBluetoothDeviceNameForDisplay(info));
}

}  // namespace device
