// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/extensions/system_web_app_manager_browsertest.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/theme_provider.h"

class BrowserNonClientFrameViewBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  BrowserNonClientFrameViewBrowserTest() = default;
  ~BrowserNonClientFrameViewBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    extensions::ExtensionBrowserTest::SetUp();
  }

  // Note: A "bookmark app" is a type of hosted app. All of these tests apply
  // equally to hosted and bookmark apps, but it's easier to install a bookmark
  // app in a test.
  void InstallAndLaunchBookmarkApp() {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GetAppURL();
    web_app_info.scope = GetAppURL().GetWithoutFilename();
    if (app_theme_color_)
      web_app_info.theme_color = *app_theme_color_;

    const extensions::Extension* app =
        extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                         web_app_info);
    app_browser_ = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), app);
    web_contents_ = app_browser_->tab_strip_model()->GetActiveWebContents();
    // Ensure the main page has loaded and is ready for ExecJs DOM manipulation.
    ASSERT_TRUE(content::NavigateToURL(web_contents_, GetAppURL()));

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(app_browser_);
    app_frame_view_ = browser_view->frame()->GetFrameView();
  }

 protected:
  base::Optional<SkColor> app_theme_color_ = SK_ColorBLUE;
  Browser* app_browser_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  BrowserNonClientFrameView* app_frame_view_ = nullptr;

 private:
  GURL GetAppURL() { return embedded_test_server()->GetURL("/empty.html"); }

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewBrowserTest);
};

// Tests the frame color for a normal browser window.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BrowserFrameColorThemed) {
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  const BrowserNonClientFrameView* frame_view =
      browser_view->frame()->GetFrameView();
  const ui::ThemeProvider* theme_provider = frame_view->GetThemeProvider();
  const SkColor expected_active_color =
      theme_provider->GetColor(ThemeProperties::COLOR_FRAME);
  const SkColor expected_inactive_color =
      theme_provider->GetColor(ThemeProperties::COLOR_FRAME_INACTIVE);

  EXPECT_EQ(expected_active_color,
            frame_view->GetFrameColor(BrowserNonClientFrameView::kActive));
  EXPECT_EQ(expected_inactive_color,
            frame_view->GetFrameColor(BrowserNonClientFrameView::kInactive));
}

// Tests the frame color for a bookmark app when a theme is applied.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BookmarkAppFrameColorCustomTheme) {
  // The theme color should not affect the window, but the theme must not be the
  // default GTK theme for Linux so we install one anyway.
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  InstallAndLaunchBookmarkApp();
  // Note: This is checking for the bookmark app's theme color, not the user's
  // theme color.
  EXPECT_EQ(*app_theme_color_,
            app_frame_view_->GetFrameColor(BrowserNonClientFrameView::kActive));
}

// Tests the frame color for a bookmark app when a theme is applied, with the
// app itself having no theme color.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BookmarkAppFrameColorCustomThemeNoThemeColor) {
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  app_theme_color_.reset();
  InstallAndLaunchBookmarkApp();
  // Bookmark apps are not affected by browser themes.
  EXPECT_EQ(
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_FRAME, false),
      app_frame_view_->GetFrameColor(BrowserNonClientFrameView::kActive));
}

// Tests the frame color for a bookmark app when the system theme is applied.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BookmarkAppFrameColorSystemTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  // Should be using the system theme by default, but this assert was not true
  // on the bots. Explicitly set.
  theme_service->UseSystemTheme();
  ASSERT_TRUE(theme_service->UsingSystemTheme());

  InstallAndLaunchBookmarkApp();
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // On Linux, the system theme is the GTK theme and should change the frame
  // color to the system color (not the app theme color); otherwise the title
  // and border would clash horribly with the GTK title bar.
  // (https://crbug.com/878636)
  const ui::ThemeProvider* theme_provider = app_frame_view_->GetThemeProvider();
  const SkColor frame_color =
      theme_provider->GetColor(ThemeProperties::COLOR_FRAME);
  EXPECT_EQ(frame_color,
            app_frame_view_->GetFrameColor(BrowserNonClientFrameView::kActive));
#else
  EXPECT_EQ(*app_theme_color_,
            app_frame_view_->GetFrameColor(BrowserNonClientFrameView::kActive));
#endif
}

using SystemWebAppNonClientFrameViewBrowserTest =
    web_app::SystemWebAppManagerBrowserTest;

// System Web Apps don't get the hosted app buttons.
IN_PROC_BROWSER_TEST_F(SystemWebAppNonClientFrameViewBrowserTest,
                       HideHostedAppButtonContainer) {
  Browser* app_browser =
      WaitForSystemAppInstallAndLaunch(web_app::SystemAppType::SETTINGS);
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(app_browser)
                         ->frame()
                         ->GetFrameView()
                         ->hosted_app_button_container_for_testing());
}

// Checks that the title bar for hosted app windows is hidden when in fullscreen
// for tab mode.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       FullscreenForTabTitlebarHeight) {
  InstallAndLaunchBookmarkApp();
  EXPECT_GT(app_frame_view_->GetTopInset(false), 0);

  static_cast<content::WebContentsDelegate*>(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_, web_contents_->GetURL(), {});

  EXPECT_EQ(app_frame_view_->GetTopInset(false), 0);
}

// Tests that the custom tab bar is visible in fullscreen mode.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       CustomTabBarIsVisibleInFullscreen) {
  InstallAndLaunchBookmarkApp();

  ui_test_utils::NavigateToURL(app_browser_, GURL("http://example.com"));

  static_cast<content::WebContentsDelegate*>(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_, web_contents_->GetURL(), {});

  EXPECT_TRUE(
      app_frame_view_->browser_view()->toolbar()->custom_tab_bar()->IsDrawn());
}

// Tests that hosted app frames reflect the theme color set by HTML meta tags.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       HTMLMetaThemeColorOverridesManifest) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDefaultTheme();

  InstallAndLaunchBookmarkApp();
  EXPECT_EQ(app_frame_view_->GetFrameColor(), *app_theme_color_);

  EXPECT_TRUE(content::ExecJs(web_contents_, R"(
      document.documentElement.innerHTML =
          '<meta name="theme-color" content="yellow">';
  )"));
  EXPECT_EQ(app_frame_view_->GetFrameColor(), SK_ColorYELLOW);
  DCHECK_NE(*app_theme_color_, SK_ColorYELLOW);
}
