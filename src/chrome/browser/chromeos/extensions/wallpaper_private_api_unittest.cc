// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAccount[] = "user@test.com";

}  // namespace

class WallpaperPrivateApiUnittest : public testing::Test {
 public:
  WallpaperPrivateApiUnittest()
      : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()),
        fake_user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

  WallpaperPrivateApiUnittest(const WallpaperPrivateApiUnittest&) = delete;
  WallpaperPrivateApiUnittest& operator=(const WallpaperPrivateApiUnittest&) =
      delete;

  ~WallpaperPrivateApiUnittest() override = default;

  void SetUp() override {
    // Required for WallpaperControllerClientImpl.
    chromeos::SystemSaltGetter::Initialize();
  }

  void TearDown() override {
    chromeos::SystemSaltGetter::Shutdown();
  }

 protected:
  ash::FakeChromeUserManager* fake_user_manager() { return fake_user_manager_; }

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  ash::FakeChromeUserManager* fake_user_manager_;

  user_manager::ScopedUserManager scoped_user_manager_;
};

// Test wallpaperPrivate.resetWallpaper() function. Regression test for
// https://crbug.com/830157.
TEST_F(WallpaperPrivateApiUnittest, ResetWallpaper) {
  chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting(
      chromeos::SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));

  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  TestWallpaperController test_controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&test_controller);
  fake_user_manager()->AddUser(AccountId::FromUserEmail(kTestAccount));

  {
    auto function =
        base::MakeRefCounted<WallpaperPrivateResetWallpaperFunction>();
    EXPECT_TRUE(
        extensions::api_test_utils::RunFunction(function.get(), "[]", nullptr));
  }

  // Expect SetDefaultWallpaper() to be called exactly once.
  EXPECT_EQ(1, test_controller.set_default_wallpaper_count());
}
