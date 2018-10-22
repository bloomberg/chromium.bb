// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/install_limiter.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::InstallLimiter;

namespace {

constexpr char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
constexpr int kLargeExtensionSize = 2000000;
constexpr int kSmallExtensionSize = 200000;

}  // namespace

class InstallLimiterTest : public testing::Test {
 public:
  InstallLimiterTest() = default;
  ~InstallLimiterTest() override = default;

  void SetUp() override {
    auto image_loader_client =
        std::make_unique<chromeos::FakeImageLoaderClient>();
    image_loader_client_ = image_loader_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
        std::move(image_loader_client));
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void TearDown() override {
    chromeos::DemoSession::ShutDownIfInitialized();
    chromeos::DemoSession::ResetDemoConfigForTesting();
    image_loader_client_ = nullptr;
    chromeos::DBusThreadManager::Shutdown();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  // Points to the image loader client passed to the test DBusTestManager.
  chromeos::FakeImageLoaderClient* image_loader_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiterTest);
};

TEST_F(InstallLimiterTest, ShouldDeferInstall) {
  const std::vector<std::string> screensaver_ids = {
      extension_misc::kScreensaverAppId, extension_misc::kScreensaverAlt1AppId,
      extension_misc::kScreensaverAlt2AppId};
  // In non-demo mode, all apps larger than 1 MB should be deferred.
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);
  for (const std::string& id : screensaver_ids) {
    EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  // In demo mode (either online or offline), all apps larger than 1MB except
  // for the screensaver should be deferred.
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();
  for (const std::string& id : screensaver_ids) {
    EXPECT_NE(id == chromeos::DemoSession::GetScreensaverAppId(),
              InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  for (const std::string& id : screensaver_ids) {
    EXPECT_NE(id == chromeos::DemoSession::GetScreensaverAppId(),
              InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  chromeos::DemoSession::ResetDemoConfigForTesting();
}