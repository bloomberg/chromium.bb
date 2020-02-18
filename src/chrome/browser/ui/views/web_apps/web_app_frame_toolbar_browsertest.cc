// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_test.h"

#include <cmath>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "content/public/common/page_zoom.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

#if defined(OS_MACOSX)
// Keep in sync with browser_non_client_frame_view_mac.mm
constexpr double kTitlePaddingWidthFraction = 0.1;
#endif

}  // namespace

class WebAppFrameToolbarBrowserTest : public WebAppFrameToolbarTest {
 public:
  WebAppFrameToolbarBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    WebAppFrameToolbarTest::SetUp();
  }

 private:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, SpaceConstrained) {
  const GURL app_url("https://test.org");
  InstallAndLaunchWebApp(app_url);

  views::View* const toolbar_left_container =
      web_app_frame_toolbar()->GetLeftContainerForTesting();
  EXPECT_EQ(toolbar_left_container->parent(), web_app_frame_toolbar());

  views::View* const window_title =
      frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE);
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(window_title);
#else
  EXPECT_EQ(window_title->parent(), frame_view());
#endif

  views::View* const toolbar_right_container =
      web_app_frame_toolbar()->GetRightContainerForTesting();
  EXPECT_EQ(toolbar_right_container->parent(), web_app_frame_toolbar());

  views::View* const page_action_icon_container =
      web_app_frame_toolbar()->GetPageActionIconContainerForTesting();
  EXPECT_EQ(page_action_icon_container->parent(), toolbar_right_container);

  views::View* const menu_button =
      browser_view()->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), toolbar_right_container);

  // Ensure we initially have abundant space.
  frame_view()->SetSize(gfx::Size(1000, 1000));

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  const int original_left_container_width = toolbar_left_container->width();
  EXPECT_GT(original_left_container_width, 0);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  const int original_window_title_width = window_title->width();
  EXPECT_GT(original_window_title_width, 0);
#endif

  // Initially the page action icons are not visible.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  const int original_menu_button_width = menu_button->width();
  EXPECT_GT(original_menu_button_width, 0);

  // Cause the zoom page action icon to be visible.
  chrome::Zoom(app_browser(), content::PAGE_ZOOM_IN);

  // The layout should be invalidated, but since we don't have the benefit of
  // the compositor to immediately kick a layout off, we have to do it manually.
  frame_view()->Layout();

  // The page action icons should now take up width, leaving less space on
  // Windows and Linux for the window title. (On Mac, the window title remains
  // centered - not tested here.)

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  EXPECT_EQ(toolbar_left_container->width(), original_left_container_width);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_GT(window_title->width(), 0);
  EXPECT_LT(window_title->width(), original_window_title_width);
#endif

  EXPECT_GT(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);

  // Resize the WebAppFrameToolbarView just enough to clip out the page action
  // icons (and toolbar contents left of them).
  const int original_toolbar_width = web_app_frame_toolbar()->width();
  web_app_frame_toolbar()->SetSize(
      gfx::Size(toolbar_right_container->width() -
                    page_action_icon_container->bounds().right(),
                web_app_frame_toolbar()->height()));
  frame_view()->SetSize(gfx::Size(frame_view()->width() -
                                      original_toolbar_width +
                                      web_app_frame_toolbar()->width(),
                                  frame_view()->height()));

  // The left container (containing Back and Reload) should be hidden.
  EXPECT_FALSE(toolbar_left_container->GetVisible());

  // The window title should be clipped to 0 width.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_EQ(window_title->width(), 0);
#endif

  // The page action icons should be clipped to 0 width while the app menu
  // button retains its full width.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);
}

// Test that a tooltip is shown when hovering over a truncated title.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, TitleHover) {
  const GURL app_url("https://test.org");
  InstallAndLaunchWebApp(app_url);

  views::View* const toolbar_left_container =
      web_app_frame_toolbar()->GetLeftContainerForTesting();
  views::View* const toolbar_right_container =
      web_app_frame_toolbar()->GetRightContainerForTesting();

  auto* const window_title = static_cast<views::Label*>(
      frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE));
#if defined(OS_CHROMEOS)
  // Chrome OS PWA windows do not display app titles.
  EXPECT_EQ(nullptr, window_title);
  return;
#endif
  EXPECT_EQ(window_title->parent(), frame_view());

  window_title->SetText(base::string16(30, 't'));

  // Ensure we initially have abundant space.
  frame_view()->SetSize(gfx::Size(1000, 1000));
  frame_view()->Layout();
  EXPECT_GT(window_title->width(), 0);
  const int original_title_gap = toolbar_right_container->x() -
                                 toolbar_left_container->x() -
                                 toolbar_left_container->width();

  // With a narrow window, we have insufficient space for the full title.
  const int narrow_title_gap =
      window_title->CalculatePreferredSize().width() * 3 / 4;
  int narrow_frame_width =
      frame_view()->width() - original_title_gap + narrow_title_gap;
#if defined(OS_MACOSX)
  // Increase frame width to allow for title padding.
  narrow_frame_width = base::checked_cast<int>(
      std::ceil(narrow_frame_width / (1 - 2 * kTitlePaddingWidthFraction)));
#endif
  frame_view()->SetSize(gfx::Size(narrow_frame_width, 1000));
  frame_view()->Layout();

  EXPECT_GT(window_title->width(), 0);
  EXPECT_EQ(window_title->GetTooltipHandlerForPoint(gfx::Point(0, 0)),
            window_title);

  EXPECT_EQ(frame_view()->GetTooltipHandlerForPoint(window_title->origin()),
            window_title);
}
