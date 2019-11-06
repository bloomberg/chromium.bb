// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/manifest_web_app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

using ManifestWebAppTest = InProcessBrowserTest;

// Opens a basic example site in focus mode window.
IN_PROC_BROWSER_TEST_F(ManifestWebAppTest, OpenExampleSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFocusMode);
  const GURL url("http://example.org/");
  ui_test_utils::NavigateToURL(browser(), url);
  Browser* app_browser = ReparentWebContentsForFocusMode(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(app_browser->is_focus_mode());
  ASSERT_FALSE(browser()->is_focus_mode());

  std::unique_ptr<ManifestWebAppBrowserController> controller =
      std::make_unique<ManifestWebAppBrowserController>(app_browser);
  controller->UpdateToolbarVisibility(false);
  // http://example.org is not a secure site, so show toolbar about site
  // information that warn users.
  EXPECT_TRUE(controller->ShouldShowToolbar());
  // Theme color should be default color (white).
  EXPECT_EQ(controller->GetThemeColor(), SK_ColorWHITE);
}
