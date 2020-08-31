// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/assistant/device_actions_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::assistant::mojom::AndroidAppInfo;
using chromeos::assistant::mojom::AndroidAppInfoPtr;
using chromeos::assistant::mojom::AppStatus;

namespace {

constexpr char kRegisteredAppName[] = "registered_app_name";
constexpr char kOtherRegisteredAppName[] = "other_registered_app_name";
constexpr char kUnregisteredAppName[] = "unregistered_app_name";

class FakeDeviceActionsDelegate : public DeviceActionsDelegate {
  AppStatus GetAndroidAppStatus(const std::string& package_name) override {
    return apps_.find(package_name) != apps_.end() ? AppStatus::AVAILABLE
                                                   : AppStatus::UNAVAILABLE;
  }

 private:
  std::set<std::string> apps_ = {kRegisteredAppName, kOtherRegisteredAppName};
};

}  // namespace

class DeviceActionsTest : public ChromeAshTestBase {
 public:
  DeviceActionsTest() {}
  ~DeviceActionsTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    device_actions_ = std::make_unique<DeviceActions>(
        std::make_unique<FakeDeviceActionsDelegate>());
  }

  void TearDown() override {
    device_actions_.reset();
    ChromeAshTestBase::TearDown();
  }

  DeviceActions* device_actions() { return device_actions_.get(); }

  AppStatus GetAppStatus(std::string package_name) {
    AndroidAppInfoPtr app_info = AndroidAppInfo::New();
    app_info->package_name = package_name;

    return device_actions()->GetAndroidAppStatus(*app_info);
  }

 private:
  std::unique_ptr<DeviceActions> device_actions_;
};

TEST_F(DeviceActionsTest, RegisteredAppShouldBeAvailable) {
  ASSERT_EQ(GetAppStatus(kRegisteredAppName), AppStatus::AVAILABLE);
}

TEST_F(DeviceActionsTest, UnregisteredAppShouldBeUnavailable) {
  ASSERT_EQ(GetAppStatus(kUnregisteredAppName), AppStatus::UNAVAILABLE);
}

TEST_F(DeviceActionsTest, UnknownAppShouldBeUnknown) {
}

TEST_F(DeviceActionsTest, MultipleAppsShouldBeVerifiedCorrectly) {
  ASSERT_EQ(GetAppStatus(kRegisteredAppName), AppStatus::AVAILABLE);
  ASSERT_EQ(GetAppStatus(kUnregisteredAppName), AppStatus::UNAVAILABLE);
  ASSERT_EQ(GetAppStatus(kOtherRegisteredAppName), AppStatus::AVAILABLE);
}
