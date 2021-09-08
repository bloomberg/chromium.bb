// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace {

// Tests of window placement for popup browser windows. Test fixtures are run
// with and without the experimental WindowPlacement blink feature.
class PopupBrowserTest : public InProcessBrowserTest,
                         public ::testing::WithParamInterface<bool> {
 protected:
  PopupBrowserTest() = default;
  ~PopupBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
    const bool enable_window_placement = GetParam();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        enable_window_placement ? switches::kEnableBlinkFeatures
                                : switches::kDisableBlinkFeatures,
        "WindowPlacement");
  }

  display::Display GetDisplayNearestBrowser(const Browser* browser) const {
    return display::Screen::GetScreen()->GetDisplayNearestWindow(
        browser->window()->GetNativeWindow());
  }

  Browser* OpenPopup(Browser* browser, const std::string& script) const {
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();
    content::ExecuteScriptAsync(contents, script);
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(popup, browser);
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetMainFrame()));
    return popup;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All, PopupBrowserTest, ::testing::Bool());

// A helper class to wait for widget bounds changes beyond given thresholds.
class WidgetBoundsChangeWaiter final : public views::WidgetObserver {
 public:
  WidgetBoundsChangeWaiter(views::Widget* widget, int move_by, int resize_by)
      : widget_(widget),
        move_by_(move_by),
        resize_by_(resize_by),
        initial_bounds_(widget->GetWindowBoundsInScreen()) {
    widget_->AddObserver(this);
  }

  WidgetBoundsChangeWaiter(const WidgetBoundsChangeWaiter&) = delete;
  WidgetBoundsChangeWaiter& operator=(const WidgetBoundsChangeWaiter&) = delete;
  ~WidgetBoundsChangeWaiter() final { widget_->RemoveObserver(this); }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& rect) final {
    if (BoundsChangeMeetsThreshold(rect)) {
      widget_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Wait for changes to occur, or return immediately if they already have.
  void Wait() {
    if (!BoundsChangeMeetsThreshold(widget_->GetWindowBoundsInScreen()))
      run_loop_.Run();
  }

 private:
  bool BoundsChangeMeetsThreshold(const gfx::Rect& rect) const {
    return (std::abs(rect.x() - initial_bounds_.x()) >= move_by_ ||
            std::abs(rect.y() - initial_bounds_.y()) >= move_by_) &&
           (std::abs(rect.width() - initial_bounds_.width()) >= resize_by_ ||
            std::abs(rect.height() - initial_bounds_.height()) >= resize_by_);
  }

  views::Widget* const widget_;
  const int move_by_, resize_by_;
  const gfx::Rect initial_bounds_;
  base::RunLoop run_loop_;
};

// Ensure popups are opened in the available space of the opener's display.
// TODO(crbug.com/1211516): Flaky.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, DISABLED_OpenClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  EXPECT_TRUE(display.work_area().Contains(browser()->window()->GetBounds()))
      << "The browser window should be contained by its display's work area";

  // Attempt to open a popup outside the bounds of the opener's display.
  const char* const open_scripts[] = {
      "open('.', '', 'left=' + (screen.availLeft - 50));",
      "open('.', '', 'left=' + (screen.availLeft + screen.availWidth + 50));",
      "open('.', '', 'top=' + (screen.availTop - 50));",
      "open('.', '', 'top=' + (screen.availTop + screen.availHeight + 50));",
      "open('.', '', 'left=' + (screen.availLeft - 50) + "
      "',top=' + (screen.availTop - 50));",
      "open('.', '', 'left=' + (screen.availLeft - 50) + "
      "',top=' + (screen.availTop - 50) + "
      "',width=300,height=300');",
      "open('.', '', 'left=' + (screen.availLeft + screen.availWidth + 50) + "
      "',top=' + (screen.availTop + screen.availHeight + 50) + "
      "',width=300,height=300');",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=' + (screen.availWidth + 300) + ',height=300');",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=300,height='+ (screen.availHeight + 300));",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=' + (screen.availWidth + 300) + "
      "',height='+ (screen.availHeight + 300));",
  };
  for (auto* const script : open_scripts) {
    Browser* popup = OpenPopup(browser(), script);
    // The popup should be constrained to the opener's available display space.
    // TODO(crbug.com/897300): Wait for the final window placement to occur;
    // this is flakily checking initial or intermediate window placement bounds.
    EXPECT_EQ(display, GetDisplayNearestBrowser(popup));
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be moved beyond the available display space by script.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, MoveClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  const char kOpenPopup[] =
      "open('.', '', 'left=' + (screen.availLeft + 50) + "
      "',top=' + (screen.availTop + 50) + "
      "',width=150,height=100');";
  const char* const kMoveScripts[] = {
      "moveBy(screen.availWidth * 2, 0);",
      "moveBy(screen.availWidth * -2, 0);",
      "moveBy(0, screen.availHeight * 2);",
      "moveBy(0, screen.availHeight * -2);",
      "moveBy(screen.availWidth * 2, screen.availHeight * 2);",
      "moveBy(screen.availWidth * -2, screen.availHeight * -2);",
      "moveTo(screen.availLeft + screen.availWidth + 50, screen.availTop);",
      "moveTo(screen.availLeft - 50, screen.availTop);",
      "moveTo(screen.availLeft, screen.availTop + screen.availHeight + 50);",
      "moveTo(screen.availLeft, screen.availTop - 50);",
      ("moveTo(screen.availLeft + screen.availWidth + 50, "
       "screen.availTop + screen.availHeight + 50);"),
      "moveTo(screen.availLeft - 50, screen.availTop - 50);",
  };
  for (auto* const script : kMoveScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    auto* widget = views::Widget::GetWidgetForNativeWindow(
        popup->window()->GetNativeWindow());

    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for the substantial move, widgets may move during initialization.
    WidgetBoundsChangeWaiter(widget, /*move_by=*/40, /*resize_by=*/0).Wait();
    EXPECT_NE(popup_bounds.origin(), popup->window()->GetBounds().origin());
    EXPECT_EQ(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup_bounds.ToString();
  }
}

// Ensure popups cannot be resized beyond the available display space by script.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, ResizeClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  const char kOpenPopup[] =
      "open('.', '', 'left=' + (screen.availLeft + 50) + "
      "',top=' + (screen.availTop + 50) + "
      "',width=150,height=100');";
  // The popup cannot be resized beyond the current screen by script.
  const char* const kResizeScripts[] = {
      "resizeBy(screen.availWidth * 2, 0);",
      "resizeBy(0, screen.availHeight * 2);",
      "resizeTo(screen.availWidth + 200, 200);",
      "resizeTo(200, screen.availHeight + 200);",
      "resizeTo(screen.availWidth + 200, screen.availHeight + 200);",
  };
  for (auto* const script : kResizeScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    auto* widget = views::Widget::GetWidgetForNativeWindow(
        popup->window()->GetNativeWindow());

    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for the substantial resize, widgets may move during initialization.
    WidgetBoundsChangeWaiter(widget, /*move_by=*/0, /*resize_by=*/100).Wait();
    EXPECT_NE(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup_bounds.ToString();
  }
}

}  // namespace
