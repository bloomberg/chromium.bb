// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <tuple>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
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
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
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
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_WIN) ||                                   \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
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

#if BUILDFLAG(IS_WIN) ||                                   \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
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
#if BUILDFLAG(IS_WIN) ||                                   \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
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
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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
#if BUILDFLAG(IS_MAC)
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
  class TestInfoBarDelegate : public infobars::InfoBarDelegate {
   public:
    static infobars::InfoBar* Create(
        infobars::ContentInfoBarManager* infobar_manager) {
      return static_cast<InfoBarView*>(
          infobar_manager->AddInfoBar(std::make_unique<InfoBarView>(
              std::make_unique<TestInfoBarDelegate>())));
    }

    TestInfoBarDelegate() = default;
    ~TestInfoBarDelegate() override = default;

    // infobars::InfoBarDelegate:
    infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier()
        const override {
      return InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
    }
  };

  WebAppFrameToolbarBrowserTest_WindowControlsOverlay() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAppWindowControlsOverlay);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  web_app::AppId InstallAndLaunchWebApp() {
    EXPECT_TRUE(https_server()->Start());
    GURL start_url =
        helper()->LoadWindowControlsOverlayTestPageWithDataAndGetURL(
            embedded_test_server(), &temp_dir_);

    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.emplace_back(
        web_app::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A window-controls-overlay app";
    web_app_info->display_mode = web_app::DisplayMode::kStandalone;
    web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->display_override = display_overrides;

    return helper()->InstallAndLaunchCustomWebApp(
        browser(), std::move(web_app_info), start_url);
  }

  void ToggleWindowControlsOverlayAndWait() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    helper()->browser_view()->ToggleWindowControlsOverlayEnabled();
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  bool GetWindowControlOverlayVisibility() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    return EvalJs(web_contents,
                  "window.navigator.windowControlsOverlay.visible")
        .ExtractBool();
  }

  bool GetWindowControlOverlayVisibilityFromEvent() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    return EvalJs(web_contents, "overlay_visible_from_event").ExtractBool();
  }

  void ShowInfoBarAndWait() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    TestInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(
            helper()
                ->app_browser()
                ->tab_strip_model()
                ->GetActiveWebContents()));
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRect() {
    const std::string kRectValueList =
        "var rect = "
        "[navigator.windowControlsOverlay.getTitlebarAreaRect().x, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().y, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().width, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().height];";
    return helper()->GetXYWidthHeightRect(
        helper()->browser_view()->GetActiveWebContents(), kRectValueList,
        "rect");
  }

  std::string GetCSSTitlebarRect() {
    return "var element = document.getElementById('target');"
           "var titlebarAreaX = "
           "    getComputedStyle(element).getPropertyValue('padding-left');"
           "var titlebarAreaXInt = parseInt(titlebarAreaX.split('px')[0]);"
           "var titlebarAreaY = "
           "    getComputedStyle(element).getPropertyValue('padding-top');"
           "var titlebarAreaYInt = parseInt(titlebarAreaY.split('px')[0]);"
           "var titlebarAreaWidthRect = "
           "    getComputedStyle(element).getPropertyValue('padding-right');"
           "var titlebarAreaWidthRectInt = "
           "    parseInt(titlebarAreaWidthRect.split('px')[0]);"
           "var titlebarAreaHeightRect = "
           "    getComputedStyle(element).getPropertyValue('padding-bottom');"
           "var titlebarAreaHeightRectInt = "
           "    parseInt(titlebarAreaHeightRect.split('px')[0]);";
  }

  void ResizeWindowBoundsAndWait(const gfx::Rect& new_bounds) {
    // Changing the width of widget should trigger a "geometrychange" event.
    EXPECT_NE(new_bounds.width(),
              helper()->browser_view()->GetLocalBounds().width());

    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRectFromEvent() {
    const std::string kRectValueList =
        "var rect = [overlay_rect_from_event.x, overlay_rect_from_event.y, "
        "overlay_rect_from_event.width, overlay_rect_from_event.height];";

    return helper()->GetXYWidthHeightRect(
        helper()->browser_view()->GetActiveWebContents(), kRectValueList,
        "rect");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       WindowControlsOverlay) {
  InstallAndLaunchWebApp();

  // Toggle overlay on, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayAndWait();

  gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

#if BUILDFLAG(IS_MAC)
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
#else
  EXPECT_EQ(gfx::Point(), bounds.origin());
#endif
  EXPECT_FALSE(bounds.IsEmpty());

  // Toggle overlay off, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayAndWait();
  bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  EXPECT_EQ(gfx::Rect(), bounds);
}

// TODO(crbug.com/1263672) Enable for LaCrOS when the blocker bug has been
// fixed.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       GeometryChangeEvent) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  // Store the initial bounding client rect for comparison later.
  const gfx::Rect initial_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() - 1);
  ResizeWindowBoundsAndWait(new_bounds);

  // Validate both the event payload and JS bounding client rect reflect
  // the new size.
  const gfx::Rect resized_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  const gfx::Rect resized_js_overlay_event_bounds =
      GetWindowControlOverlayBoundingClientRectFromEvent();
  EXPECT_EQ(1, EvalJs(helper()->browser_view()->GetActiveWebContents(),
                      "geometrychangeCount"));
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_TRUE(GetWindowControlOverlayVisibilityFromEvent());
  EXPECT_EQ(resized_js_overlay_bounds, resized_js_overlay_event_bounds);
  EXPECT_EQ(initial_js_overlay_bounds.origin(),
            resized_js_overlay_bounds.origin());
  EXPECT_NE(initial_js_overlay_bounds.width(),
            resized_js_overlay_bounds.width());
  EXPECT_EQ(initial_js_overlay_bounds.height(),
            resized_js_overlay_bounds.height());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       NoGeometryChangeEventIfOverlayIsOff) {
  InstallAndLaunchWebApp();

  constexpr char kTestScript[] =
      "document.title = 'beforeevent';"
      "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
      "  document.title = 'ongeometrychange';"
      "};"
      "window.onresize = (e) => {"
      "  document.title = 'onresize';"
      "};";

  // Window Controls Overlay is off by default.
  ASSERT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());

  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 10);
  content::TitleWatcher title_watcher(web_contents, u"onresize");
  EXPECT_TRUE(ExecJs(web_contents->GetMainFrame(), kTestScript));
  helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
  title_watcher.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher.WaitAndGetTitle());

  // Toggle Window Control Overlay on and then off.
  ToggleWindowControlsOverlayAndWait();
  ToggleWindowControlsOverlayAndWait();

  // Validate event is not fired.
  new_bounds.set_width(new_bounds.width() - 10);
  content::TitleWatcher title_watcher2(web_contents, u"onresize");
  EXPECT_TRUE(ExecJs(web_contents->GetMainFrame(), kTestScript));
  helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
  title_watcher2.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher2.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       WindowControlsOverlayRTL) {
  base::test::ScopedRestoreICUDefaultLocale test_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  const gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
  EXPECT_FALSE(bounds.IsEmpty());
}

// TODO(crbug.com/1263672) Enable for LaCrOS when the blocker bug has been
// fixed.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       CSSRectTestLTR) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  std::string kCSSTitlebarRect = GetCSSTitlebarRect();
  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kCSSTitlebarRect));

  const std::string kRectListString =
      "var rect = [titlebarAreaXInt, titlebarAreaYInt, "
      "titlebarAreaWidthRectInt, "
      "titlebarAreaHeightRectInt];";

  base::Value::ListStorage initial_rect_list =
      helper()->GetXYWidthHeightListValue(
          helper()->browser_view()->GetActiveWebContents(), kRectListString,
          "rect");

  const int initial_x_value = initial_rect_list[0].GetInt();
  const int initial_y_value = initial_rect_list[1].GetInt();
  const int initial_width_value = initial_rect_list[2].GetInt();
  const int initial_height_value = initial_rect_list[3].GetInt();

#if BUILDFLAG(IS_MAC)
  // Window controls are on the opposite side on Mac.
  EXPECT_NE(0, initial_x_value);
#else
  EXPECT_EQ(0, initial_x_value);
#endif
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 20);
  new_bounds.set_height(new_bounds.height() + 20);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kCSSTitlebarRect));

  base::Value::ListStorage updated_rect_list =
      helper()->GetXYWidthHeightListValue(
          helper()->browser_view()->GetActiveWebContents(), kRectListString,
          "rect");

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_rect_list[0].GetInt());
  EXPECT_EQ(initial_y_value, updated_rect_list[1].GetInt());
  EXPECT_NE(initial_width_value, updated_rect_list[2].GetInt());
  EXPECT_EQ(initial_height_value, updated_rect_list[3].GetInt());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       CSSRectTestRTL) {
  base::test::ScopedRestoreICUDefaultLocale test_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  std::string kCSSTitlebarRect = GetCSSTitlebarRect();
  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kCSSTitlebarRect));

  const std::string kRectListString =
      "var rect = [titlebarAreaXInt, titlebarAreaYInt, "
      "titlebarAreaWidthRectInt, "
      "titlebarAreaHeightRectInt];";

  base::Value::ListStorage initial_rect_list =
      helper()->GetXYWidthHeightListValue(
          helper()->browser_view()->GetActiveWebContents(), kRectListString,
          "rect");

  const int initial_x_value = initial_rect_list[0].GetInt();
  const int initial_y_value = initial_rect_list[1].GetInt();
  const int initial_width_value = initial_rect_list[2].GetInt();
  const int initial_height_value = initial_rect_list[3].GetInt();

  EXPECT_NE(0, initial_x_value);
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 15);
  new_bounds.set_height(new_bounds.height() + 15);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kCSSTitlebarRect));

  base::Value::ListStorage updated_rect_list =
      helper()->GetXYWidthHeightListValue(
          helper()->browser_view()->GetActiveWebContents(), kRectListString,
          "rect");

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_rect_list[0].GetInt());
  EXPECT_EQ(initial_y_value, updated_rect_list[1].GetInt());
  EXPECT_NE(initial_width_value, updated_rect_list[2].GetInt());
  EXPECT_EQ(initial_height_value, updated_rect_list[3].GetInt());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(https://crbug.com/1277860): Flaky.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DISABLED_WindowControlsOverlayDraggableRegions) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  views::NonClientFrameView* frame_view =
      helper()->browser_view()->GetWidget()->non_client_view()->frame_view();

  // Validate that a point marked "app-region: drag" is draggable. The draggable
  // region is defined in the kTestHTML of WebAppFrameToolbarTestHelper's
  // LoadWindowControlsOverlayTestPageWithDataAndGetURL.
  gfx::Point draggable_point(100, 100);
  views::View::ConvertPointToTarget(
      helper()->browser_view()->contents_web_view(), frame_view,
      &draggable_point);

  EXPECT_EQ(frame_view->NonClientHitTest(draggable_point), HTCAPTION);

  EXPECT_FALSE(helper()->browser_view()->ShouldDescendIntoChildForEventHandling(
      helper()->browser_view()->GetWidget()->GetNativeView(), draggable_point));

  // Validate that a point marked "app-region: no-drag" within a draggable
  // region is not draggable.
  gfx::Point non_draggable_point(106, 106);
  views::View::ConvertPointToTarget(
      helper()->browser_view()->contents_web_view(), frame_view,
      &non_draggable_point);

  EXPECT_EQ(frame_view->NonClientHitTest(non_draggable_point), HTCLIENT);

  EXPECT_TRUE(helper()->browser_view()->ShouldDescendIntoChildForEventHandling(
      helper()->browser_view()->GetWidget()->GetNativeView(),
      non_draggable_point));

  // Validate that a point at the border that does not intersect with the
  // overlays is not marked as draggable.
  constexpr gfx::Point kBorderPoint(100, 1);
  EXPECT_NE(frame_view->NonClientHitTest(kBorderPoint), HTCAPTION);
  EXPECT_TRUE(helper()->browser_view()->ShouldDescendIntoChildForEventHandling(
      helper()->browser_view()->GetWidget()->GetNativeView(), kBorderPoint));

  // Validate that draggable region does not clear after history.replaceState is
  // invoked.
  std::string kHistoryReplaceState =
      "history.replaceState({ test: 'test' }, null);";
  EXPECT_TRUE(ExecuteScript(helper()->browser_view()->GetActiveWebContents(),
                            kHistoryReplaceState));
  EXPECT_FALSE(helper()->browser_view()->ShouldDescendIntoChildForEventHandling(
      helper()->browser_view()->GetWidget()->GetNativeView(), draggable_point));

  // Validate that the draggable region is reset on navigation and the point is
  // no longer draggable.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(helper()->browser_view()->browser(),
                                           GURL("http://example.test/")));
  EXPECT_NE(frame_view->NonClientHitTest(draggable_point), HTCAPTION);
  EXPECT_TRUE(helper()->browser_view()->ShouldDescendIntoChildForEventHandling(
      helper()->browser_view()->GetWidget()->GetNativeView(), draggable_point));
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       ToggleWindowControlsOverlay) {
  InstallAndLaunchWebApp();

  // Make sure the app launches in standalone mode by default.
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  // Toggle WCO on, and verify that the UI updates accordingly.
  helper()->browser_view()->ToggleWindowControlsOverlayEnabled();
  EXPECT_TRUE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  // Toggle WCO off, and verify that the app returns to 'standalone' mode.
  helper()->browser_view()->ToggleWindowControlsOverlayEnabled();
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       OpenInChrome) {
  InstallAndLaunchWebApp();

  // Toggle overlay on, and validate JS API reflects the expected values.
  ToggleWindowControlsOverlayAndWait();

  // Validate non-empty bounds are being sent.
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

  chrome::ExecuteCommand(helper()->browser_view()->browser(),
                         IDC_OPEN_IN_CHROME);

  // Validate bounds are cleared.
  EXPECT_EQ(false, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          "window.navigator.windowControlsOverlay.visible"));
}

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

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       HideToggleButtonWhenInfoBarIsVisible) {
  InstallAndLaunchWebApp();

  BrowserView* browser_view = helper()->browser_view();
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Start with app in Window Controls Overlay (WCO) mode and verify that the
  // toggle button is visible.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(browser_view->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show InfoBar and verify the toggle button hides.
  ShowInfoBarAndWait();
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());
  EXPECT_FALSE(browser_view->IsWindowControlsOverlayEnabled());
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
