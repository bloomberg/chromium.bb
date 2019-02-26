// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/permissions/permission_request_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"
#import "testing/gtest_mac.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"

// This file now tests the views permission bubble, on a Cocoa browser window.

namespace {

// Shows a test bubble using the permission request manager rather than any
// toolkit-specific bubble implementation.
void ShowBubble(Browser* browser) {
  auto test_api =
      std::make_unique<test::PermissionRequestManagerTestApi>(browser);
  EXPECT_TRUE(test_api->manager());

  test_api->AddSimpleRequest(CONTENT_SETTINGS_TYPE_GEOLOCATION);

  // The PermissionRequestManager displays prompts asynchronously.
  EXPECT_FALSE(test_api->GetPromptWindow());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_api->GetPromptWindow());
}

bool HasVisibleLocationBarForBrowser(Browser* browser) {
  return browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, HasLocationBarByDefault) {
  ShowBubble(browser());
  HasVisibleLocationBarForBrowser(browser());
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       TabFullscreenHasLocationBar) {
  FullscreenNotificationObserver fullscreen_observer;
  ShowBubble(browser());
  EXPECT_TRUE(HasVisibleLocationBarForBrowser(browser()));

  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  controller->EnterFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents(), GURL());
  fullscreen_observer.Wait();
  EXPECT_FALSE(HasVisibleLocationBarForBrowser(browser()));

  controller->ExitFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents());
  fullscreen_observer.Wait();
  EXPECT_TRUE(HasVisibleLocationBarForBrowser(browser()));
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, AppHasNoLocationBar) {
  Browser* app_browser = OpenExtensionAppWindow();
  // ShowBubble(app_browser) doesn't actually show a bubble for extension app
  // windows, so create one directly.
  auto prompt =
      std::make_unique<PermissionPromptImpl>(app_browser, test_delegate());
  EXPECT_FALSE(HasVisibleLocationBarForBrowser(app_browser));
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleKioskBrowserTest,
                       KioskHasNoLocationBar) {
  ShowBubble(browser());
  // Kiosk mode on Mac has no location bar.
  EXPECT_FALSE(HasVisibleLocationBarForBrowser(browser()));
}
