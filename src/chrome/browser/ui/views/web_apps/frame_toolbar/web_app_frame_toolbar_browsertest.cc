// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

#if defined(OS_MAC)
// Keep in sync with browser_non_client_frame_view_mac.mm
constexpr double kTitlePaddingWidthFraction = 0.1;
#endif

template <typename T>
T* GetLastVisible(const std::vector<T*>& views) {
  T* visible = nullptr;
  for (auto* view : views) {
    if (view->GetVisible())
      visible = view;
  }
  return visible;
}

void LoadTestPopUpExtension(Profile* profile) {
  extensions::TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
          "name": "Pop up extension",
          "version": "1.0",
          "manifest_version": 2,
          "browser_action": {
            "default_popup": "popup.html"
          }
         })");
  test_extension_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), "");
  extensions::ChromeTestExtensionLoader(profile).LoadExtension(
      test_extension_dir.UnpackedPath());
}

}  // namespace

class WebAppFrameToolbarBrowserTest : public InProcessBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    InProcessBrowserTest::SetUp();
  }

  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }

  bool IsMenuCommandEnabled(int command_id) {
    auto app_menu_model = std::make_unique<WebAppMenuModel>(
        /*provider=*/nullptr, helper()->app_browser());
    app_menu_model->Init();
    ui::MenuModel* model = app_menu_model.get();
    int index = -1;
    if (!app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                      &index)) {
      return false;
    }
    return model->IsEnabledAt(index);
  }

 private:
  net::EmbeddedTestServer https_server_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, SpaceConstrained) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  WebAppNavigationButtonContainer* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->get_left_container_for_testing();
  EXPECT_EQ(toolbar_left_container->parent(),
            helper()->web_app_frame_toolbar());

  views::View* const window_title =
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE);
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(window_title);
#else
  EXPECT_EQ(window_title->parent(), helper()->frame_view());
#endif

  WebAppToolbarButtonContainer* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_EQ(toolbar_right_container->parent(),
            helper()->web_app_frame_toolbar());

  std::vector<const PageActionIconView*> page_actions =
      helper()
          ->web_app_frame_toolbar()
          ->GetPageActionIconControllerForTesting()
          ->GetPageActionIconViewsForTesting();
  for (const PageActionIconView* action : page_actions)
    EXPECT_EQ(action->parent(), toolbar_right_container);

  views::View* const menu_button =
      helper()->browser_view()->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), toolbar_right_container);

  // Ensure we initially have abundant space.
  helper()->frame_view()->SetSize(gfx::Size(1000, 1000));

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  const int original_left_container_width = toolbar_left_container->width();
  EXPECT_GT(original_left_container_width, 0);

#if defined(OS_WIN) || (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
                        !BUILDFLAG(IS_CHROMEOS_LACROS))
  const int original_window_title_width = window_title->width();
  EXPECT_GT(original_window_title_width, 0);
#endif

  // Initially the page action icons are not visible.
  EXPECT_EQ(GetLastVisible(page_actions), nullptr);
  const int original_menu_button_width = menu_button->width();
  EXPECT_GT(original_menu_button_width, 0);

  // Cause the zoom page action icon to be visible.
  chrome::Zoom(helper()->app_browser(), content::PAGE_ZOOM_IN);

  // The layout should be invalidated, but since we don't have the benefit of
  // the compositor to immediately kick a layout off, we have to do it manually.
  helper()->frame_view()->Layout();

  // The page action icons should now take up width, leaving less space on
  // Windows and Linux for the window title. (On Mac, the window title remains
  // centered - not tested here.)

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  EXPECT_EQ(toolbar_left_container->width(), original_left_container_width);

#if defined(OS_WIN) || (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
                        !BUILDFLAG(IS_CHROMEOS_LACROS))
  EXPECT_GT(window_title->width(), 0);
  EXPECT_LT(window_title->width(), original_window_title_width);
#endif

  EXPECT_NE(GetLastVisible(page_actions), nullptr);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);

  // Resize the WebAppFrameToolbarView just enough to clip out the page action
  // icons (and toolbar contents left of them).
  const int original_toolbar_width = helper()->web_app_frame_toolbar()->width();
  const int new_toolbar_width = toolbar_right_container->width() -
                                GetLastVisible(page_actions)->bounds().right();
  const int new_frame_width = helper()->frame_view()->width() -
                              original_toolbar_width + new_toolbar_width;

  helper()->web_app_frame_toolbar()->SetSize(
      {new_toolbar_width, helper()->web_app_frame_toolbar()->height()});
  helper()->frame_view()->SetSize(
      {new_frame_width, helper()->frame_view()->height()});

  // The left container (containing Back and Reload) should be hidden.
  EXPECT_FALSE(toolbar_left_container->GetVisible());

  // The window title should be clipped to 0 width.
#if defined(OS_WIN) || (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
                        !BUILDFLAG(IS_CHROMEOS_LACROS))
  EXPECT_EQ(window_title->width(), 0);
#endif

  // The page action icons should be hidden while the app menu button retains
  // its full width.
  EXPECT_EQ(GetLastVisible(page_actions), nullptr);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, ThemeChange) {
  ASSERT_TRUE(https_server()->Start());
  const GURL app_url = https_server()->GetURL("/banners/theme-color.html");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  content::WebContents* web_contents =
      helper()->app_browser()->tab_strip_model()->GetActiveWebContents();
  content::AwaitDocumentOnLoadCompleted(web_contents);

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // Avoid dependence on Linux GTK+ Themes appearance setting.

  ToolbarButtonProvider* const toolbar_button_provider =
      helper()->browser_view()->toolbar_button_provider();
  AppMenuButton* const app_menu_button =
      toolbar_button_provider->GetAppMenuButton();

  const SkColor original_ink_drop_color =
      views::InkDrop::Get(app_menu_button)->GetBaseColor();

  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', '#246')"));
    theme_change_waiter.Wait();

    EXPECT_NE(views::InkDrop::Get(app_menu_button)->GetBaseColor(),
              original_ink_drop_color);
  }

  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(
        web_contents, "document.getElementById('theme-color').remove()"));
    theme_change_waiter.Wait();

    EXPECT_EQ(views::InkDrop::Get(app_menu_button)->GetBaseColor(),
              original_ink_drop_color);
  }
#endif
}

// Test that a tooltip is shown when hovering over a truncated title.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, TitleHover) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  WebAppNavigationButtonContainer* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->get_left_container_for_testing();
  WebAppToolbarButtonContainer* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  auto* const window_title = static_cast<views::Label*>(
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE));
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Chrome OS PWA windows do not display app titles.
  EXPECT_EQ(nullptr, window_title);
  return;
#endif
  EXPECT_EQ(window_title->parent(), helper()->frame_view());

  window_title->SetText(std::u16string(30, 't'));

  // Ensure we initially have abundant space.
  helper()->frame_view()->SetSize(gfx::Size(1000, 1000));
  helper()->frame_view()->Layout();
  EXPECT_GT(window_title->width(), 0);
  const int original_title_gap = toolbar_right_container->x() -
                                 toolbar_left_container->x() -
                                 toolbar_left_container->width();

  // With a narrow window, we have insufficient space for the full title.
  const int narrow_title_gap =
      window_title->CalculatePreferredSize().width() * 3 / 4;
  int narrow_frame_width =
      helper()->frame_view()->width() - original_title_gap + narrow_title_gap;
#if defined(OS_MAC)
  // Increase frame width to allow for title padding.
  narrow_frame_width = base::checked_cast<int>(
      std::ceil(narrow_frame_width / (1 - 2 * kTitlePaddingWidthFraction)));
#endif
  helper()->frame_view()->SetSize(gfx::Size(narrow_frame_width, 1000));
  helper()->frame_view()->Layout();

  EXPECT_GT(window_title->width(), 0);
  EXPECT_EQ(window_title->GetTooltipHandlerForPoint(gfx::Point(0, 0)),
            window_title);

  EXPECT_EQ(
      helper()->frame_view()->GetTooltipHandlerForPoint(window_title->origin()),
      window_title);
}

class WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu
    : public WebAppFrameToolbarBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsElidedExtensionsMenu);
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu,
                       Test) {
  helper()->InstallAndLaunchWebApp(browser(), GURL("https://test.org"));

  // There should be no menu entry for opening the Extensions menu prior to
  // installing Extensions.
  EXPECT_FALSE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));

  // Install test Extension.
  LoadTestPopUpExtension(browser()->profile());

  // There should be no visible Extensions icon.
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  // There should be a menu entry for opening the Extensions menu.
  EXPECT_TRUE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));

  // Trigger the Extensions menu entry.
  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, helper()->app_browser());
  app_menu_model->Init();
  app_menu_model->ExecuteCommand(WebAppMenuModel::kExtensionsMenuCommandId,
                                 /*event_flags=*/0);

  // Extensions icon and menu should be visible.
  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());
  EXPECT_TRUE(ExtensionsMenuView::IsShowing());
}

class WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu
    : public WebAppFrameToolbarBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDesktopPWAsElidedExtensionsMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu,
                       Test) {
  helper()->InstallAndLaunchWebApp(browser(), GURL("https://test.org"));

  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Extensions toolbar should be hidden while there are no Extensions
  // installed.
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  // Install Extension and wait for Extensions toolbar to appear.
  base::RunLoop run_loop;
  ExtensionsToolbarContainer::SetOnVisibleCallbackForTesting(
      run_loop.QuitClosure());
  LoadTestPopUpExtension(browser()->profile());
  run_loop.Run();
  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());

  // There should be no menu entry for opening the Extensions menu.
  EXPECT_FALSE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));
}

class WebAppFrameToolbarBrowserTest_WindowControlsOverlay
    : public WebAppFrameToolbarBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest_WindowControlsOverlay() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAppWindowControlsOverlay);
  }

  web_app::AppId InstallAndLaunchWebApp() {
    const GURL& start_url = GURL("https://test.org");
    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.emplace_back(
        web_app::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A minimal-ui app";
    web_app_info->display_mode = web_app::DisplayMode::kStandalone;
    web_app_info->open_as_window = true;
    web_app_info->display_override = display_overrides;

    return helper()->InstallAndLaunchCustomWebApp(
        browser(), std::move(web_app_info), start_url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       HideToggleButtonWhenCCTIsVisible) {
  InstallAndLaunchWebApp();
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Start with app in standalone mode.
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  // Ensure the CCT is hidden before running checks.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ false,
                                                         /*animate*/ false);

  // Verify that the WCO toggle button shows when app is in standalone mode.
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show CCT and verify the toggle button hides.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ true,
                                                         /*animate*/ false);
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());

  // Hide CCT and enable window controls overlay.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ false,
                                                         /*animate*/ false);
  helper()->browser_view()->ToggleWindowControlsOverlayEnabled();

  // Verify that the app entered window controls overlay mode.
  EXPECT_TRUE(helper()->browser_view()->IsWindowControlsOverlayEnabled());

  // Verify that the WCO toggle button shows when app is in WCO mode.
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show CCT and verify the toggle button hides.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ true,
                                                         /*animate*/ false);
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());
}

// Regression test for https://crbug.com/1239443.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       OpenWithOverlayEnabled) {
  web_app::AppId app_id = InstallAndLaunchWebApp();
  helper()
      ->browser_view()
      ->browser()
      ->app_controller()
      ->ToggleWindowControlsOverlayEnabled();
  web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);
  // If there's no crash, the test has passed.
}
