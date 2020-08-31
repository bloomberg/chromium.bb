// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

using web_app::ControllerType;

namespace {

class TextChangeWaiter {
 public:
  explicit TextChangeWaiter(views::Label* label)
      : subscription_(label->AddTextChangedCallback(
            base::BindRepeating(&TextChangeWaiter::OnTextChanged,
                                base::Unretained(this)))) {}

  TextChangeWaiter(const TextChangeWaiter&) = delete;
  TextChangeWaiter& operator=(const TextChangeWaiter&) = delete;

  // Runs a loop until a text change is observed (unless one has
  // already been observed, in which case it returns immediately).
  void Wait() {
    if (observed_change_)
      return;

    run_loop_.Run();
  }

 private:
  void OnTextChanged() {
    observed_change_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool observed_change_ = false;
  base::RunLoop run_loop_;
  views::PropertyChangedSubscription subscription_;
};

}  // anonymous namespace

class BrowserNonClientFrameViewMacBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  BrowserNonClientFrameViewMacBrowserTest() = default;
  BrowserNonClientFrameViewMacBrowserTest(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  BrowserNonClientFrameViewMacBrowserTest& operator=(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  ~BrowserNonClientFrameViewMacBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewMacBrowserTest, TitleUpdates) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL app_url = GetInstallableAppURL();
  const web_app::AppId app_id = InstallPWA(app_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Ensure the main page has loaded and is ready for ExecJs DOM manipulation.
  ASSERT_TRUE(content::NavigateToURL(web_contents, app_url));

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::NonClientFrameView* const frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();
  auto* const title =
      static_cast<views::Label*>(frame_view->GetViewByID(VIEW_ID_WINDOW_TITLE));

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    const base::string16 expected_title(base::ASCIIToUTF16("Full Screen"));
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    const base::string16 expected_title(base::ASCIIToUTF16("Not Full Screen"));
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserNonClientFrameViewMacBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    web_app::ControllerTypeParamToString);
