// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sessions/content/content_test_helper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"

class SessionRestoreInteractiveTest : public InProcessBrowserTest {
 public:
  SessionRestoreInteractiveTest() = default;
  ~SessionRestoreInteractiveTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
  }

  bool SetUpUserDataDirectory() override {
    url1_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    Profile* profile = browser->profile();

    // Close the browser.
    std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));
    CloseBrowserSynchronously(browser);

    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
    SessionRestoreTestHelper restore_observer;

    // Ensure the session service factory is started, even if it was explicitly
    // shut down.
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfileForSessionRestore(profile));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);

    Browser* new_browser =
        chrome::FindBrowserWithWebContents(tab_waiter.Wait());

    restore_observer.Wait();
    WaitForTabsToLoad(new_browser);

    keep_alive.reset();

    return new_browser;
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      EXPECT_TRUE(content::WaitForLoadStop(contents));
    }
  }

  GURL url1_;
};

IN_PROC_BROWSER_TEST_F(SessionRestoreInteractiveTest, FocusOnLaunch) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Ensure window has initial focus on launch.
  EXPECT_TRUE(new_browser->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetRenderWidgetHostView()
                  ->HasFocus());
}
