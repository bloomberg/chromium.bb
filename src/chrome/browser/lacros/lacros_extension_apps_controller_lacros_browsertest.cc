// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include <unistd.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

using LacrosExtensionAppsControllerTest = extensions::ExtensionBrowserTest;

// Test that launching an app causing it to appear in the shelf. Closing the app
// removes it from the shelf.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, ShowsInShelf) {
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kDoesItemExistInShelfMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  LacrosExtensionAppsPublisher publisher;
  publisher.Initialize();
  LacrosExtensionAppsController controller;
  controller.Initialize(publisher.publisher());

  // No item should exist in the shelf before the window is launched.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string app_id =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  controller.Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Close all app windows.
  for (extensions::AppWindow* app_window :
       extensions::AppWindowRegistry::Get(profile())->app_windows()) {
    app_window->GetBaseWindow()->Close();
  }

  // Wait for item to stop existing in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);
}

// Test that clicking a pinned chrome app in the shelf launches it.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, LaunchPinnedApp) {
  // If ash does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kSelectItemInShelfMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  LacrosExtensionAppsPublisher publisher;
  publisher.Initialize();
  LacrosExtensionAppsController controller;
  controller.Initialize(publisher.publisher());

  // No item should exist in the shelf before the window is launched.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string app_id =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  controller.Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Pin the shelf item.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  bool success = false;
  waiter.PinOrUnpinItemInShelf(app_id, /*pin=*/true, &success);
  ASSERT_TRUE(success);

  // Close all app windows.
  for (extensions::AppWindow* app_window :
       extensions::AppWindowRegistry::Get(profile())->app_windows()) {
    std::string window_id = browser_test_util::GetWindowId(
        app_window->GetNativeWindow()->GetRootWindow());
    app_window->GetBaseWindow()->Close();
    browser_test_util::WaitForWindowDestruction(window_id);
  }

  // Confirm that there are no open windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // Clicking on the item in the shelf should launch the app again.
  success = false;
  waiter.SelectItemInShelf(app_id, &success);
  ASSERT_TRUE(success);

  // Wait for a window to open.
  base::RunLoop run_loop;
  base::RepeatingTimer timer;
  auto wait_for_window = base::BindRepeating(
      [](base::RunLoop* run_loop, Profile* profile) {
        if (!extensions::AppWindowRegistry::Get(profile)
                 ->app_windows()
                 .empty()) {
          run_loop->Quit();
        }
      },
      &run_loop, profile());
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_window));
  run_loop.Run();

  // Now we must unpin the item to ensure ash-chrome is in consistent state.
  waiter.PinOrUnpinItemInShelf(app_id, /*pin=*/false, &success);
}

// Test that the default context menu for an extension app has the correct
// items.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, DefaultContextMenu) {
  // If ash does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kGetContextMenuForShelfItemMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  LacrosExtensionAppsPublisher publisher;
  publisher.Initialize();
  LacrosExtensionAppsController controller;
  controller.Initialize(publisher.publisher());

  // No item should exist in the shelf before the window is launched.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string app_id =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  controller.Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Get the context menu.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  std::vector<std::string> items;
  waiter.GetContextMenuForShelfItem(app_id, &items);
  ASSERT_EQ(4u, items.size());
  EXPECT_EQ(items[0], "Pin to shelf");
  EXPECT_EQ(items[1], "Close");
  EXPECT_EQ(items[2], "Uninstall");
  EXPECT_EQ(items[3], "App info");
}

}  // namespace
