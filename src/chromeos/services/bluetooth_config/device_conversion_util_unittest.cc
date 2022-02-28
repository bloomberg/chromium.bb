// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_conversion_util.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {

// Tests basic usage of the GenerateBluetoothDeviceMojoProperties() conversion
// function. Not meant to be an exhaustive test of each possible Bluetooth
// property.
class DeviceConversionUtilTest : public testing::Test {
 protected:
  DeviceConversionUtilTest() = default;
  DeviceConversionUtilTest(const DeviceConversionUtilTest&) = delete;
  DeviceConversionUtilTest& operator=(const DeviceConversionUtilTest&) = delete;
  ~DeviceConversionUtilTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  }

  device::BluetoothDevice* InitDevice(uint32_t bluetooth_class,
                                      const char* name,
                                      const std::string& address,
                                      bool paired,
                                      bool connected,
                                      bool is_blocked_by_policy) {
    mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), bluetooth_class, name, address, paired,
            connected);
    mock_device_->SetIsBlockedByPolicy(is_blocked_by_policy);

    return mock_device_.get();
  }

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>> mock_device_;
};

TEST_F(DeviceConversionUtilTest, TestConversion) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/true);
  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device);
  ASSERT_TRUE(properties);
  EXPECT_EQ("address-Identifier", properties->id);
  EXPECT_EQ("address", properties->address);
  EXPECT_EQ(u"name", properties->public_name);
  EXPECT_EQ(mojom::DeviceType::kUnknown, properties->device_type);
  EXPECT_EQ(mojom::AudioOutputCapability::kNotCapableOfAudioOutput,
            properties->audio_capability);
  EXPECT_FALSE(properties->battery_info);
  EXPECT_EQ(mojom::DeviceConnectionState::kConnected,
            properties->connection_state);
  EXPECT_TRUE(properties->is_blocked_by_policy);
}

TEST_F(DeviceConversionUtilTest, TestConversion_DefaultBattery) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/false);

  device::BluetoothDevice::BatteryInfo battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::kDefault,
      /*percentage=*/65,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(battery_info);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device);
  ASSERT_TRUE(properties);

  EXPECT_TRUE(properties->battery_info->default_properties);
  EXPECT_EQ(properties->battery_info->default_properties->battery_percentage,
            65);
  EXPECT_FALSE(properties->battery_info->left_bud_info);
  EXPECT_FALSE(properties->battery_info->right_bud_info);
  EXPECT_FALSE(properties->battery_info->case_info);
}

TEST_F(DeviceConversionUtilTest, TestConversion_MultipleBatteries) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/false);

  device::BluetoothDevice::BatteryInfo left_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::
          kLeftBudTrueWireless,
      /*percentage=*/65,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(left_battery_info);

  device::BluetoothDevice::BatteryInfo right_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::
          kRightBudTrueWireless,
      /*percentage=*/45,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(right_battery_info);

  device::BluetoothDevice::BatteryInfo case_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::kCaseTrueWireless,
      /*percentage=*/50,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(case_battery_info);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device);
  ASSERT_TRUE(properties);

  EXPECT_FALSE(properties->battery_info->default_properties);
  EXPECT_TRUE(properties->battery_info->left_bud_info);
  EXPECT_EQ(properties->battery_info->left_bud_info->battery_percentage, 65);
  EXPECT_TRUE(properties->battery_info->right_bud_info);
  EXPECT_EQ(properties->battery_info->right_bud_info->battery_percentage, 45);
  EXPECT_TRUE(properties->battery_info->case_info);
  EXPECT_EQ(properties->battery_info->case_info->battery_percentage, 50);
}

}  // namespace bluetooth_config
}  // namespace chromeos
