// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client.h"

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WallpaperControllerClientTest : public testing::Test {
 public:
  WallpaperControllerClientTest() = default;
  ~WallpaperControllerClientTest() override = default;

 private:
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerClientTest);
};

TEST_F(WallpaperControllerClientTest, Construction) {
  WallpaperControllerClient client;
  TestWallpaperController controller;
  client.InitForTesting(controller.CreateInterfacePtr());
  client.FlushForTesting();

  // Singleton was initialized.
  EXPECT_EQ(&client, WallpaperControllerClient::Get());

  // Object was set as client.
  EXPECT_TRUE(controller.was_client_set());
}

}  // namespace
