// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_dock_mac_address_source_handler.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/macros.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/network/mock_network_device_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceDockMacAddressHandlerBaseTest : public testing::Test {
 public:
  DeviceDockMacAddressHandlerBaseTest() {
    scoped_cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();
    scoped_cros_settings_test_helper_.SetTrustedStatus(
        chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED);

    device_dock_mac_address_handler_ =
        std::make_unique<DeviceDockMacAddressHandler>(
            chromeos::CrosSettings::Get(), &network_device_handler_mock_);
  }

 protected:
  void MakeCrosSettingsTrusted() {
    scoped_cros_settings_test_helper_.SetTrustedStatus(
        chromeos::CrosSettingsProvider::TRUSTED);
  }

  chromeos::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;

  testing::StrictMock<chromeos::MockNetworkDeviceHandler>
      network_device_handler_mock_;

  std::unique_ptr<DeviceDockMacAddressHandler> device_dock_mac_address_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDockMacAddressHandlerBaseTest);
};

// Tests that DockMacAddressHandler does not change device handler property if
// received policy is invalid.
TEST_F(DeviceDockMacAddressHandlerBaseTest, InvalidPolicyValue) {
  MakeCrosSettingsTrusted();
  scoped_cros_settings_test_helper_.SetInteger(
      chromeos::kDeviceDockMacAddressSource, 4);
}

class DeviceDockMacAddressHandlerTest
    : public DeviceDockMacAddressHandlerBaseTest,
      public testing::WithParamInterface<std::tuple<int, std::string>> {
 public:
  DeviceDockMacAddressHandlerTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDockMacAddressHandlerTest);
};

// Tests that DockMacAddressHandler does not change device handler property if
// CrosSettings are untrusted.
TEST_P(DeviceDockMacAddressHandlerTest, OnPolicyChangeUntrusted) {
  scoped_cros_settings_test_helper_.SetInteger(
      chromeos::kDeviceDockMacAddressSource, std::get<0>(GetParam()));
}

// Tests that DockMacAddressHandler changes device handler property if
// CrosSetting are trusted.
TEST_P(DeviceDockMacAddressHandlerTest, OnPolicyChange) {
  MakeCrosSettingsTrusted();
  EXPECT_CALL(network_device_handler_mock_,
              SetUsbEthernetMacAddressSource(std::get<1>(GetParam())));
  scoped_cros_settings_test_helper_.SetInteger(
      chromeos::kDeviceDockMacAddressSource, std::get<0>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceDockMacAddressHandlerTest,
    testing::Values(std::make_tuple(1, "designated_dock_mac"),
                    std::make_tuple(2, "builtin_adapter_mac"),
                    std::make_tuple(3, "usb_adapter_mac")));

}  // namespace policy
