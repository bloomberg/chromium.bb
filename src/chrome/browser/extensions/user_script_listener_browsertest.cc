// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/notification_types.h"

namespace extensions {
namespace {

// Observer to wait for navigation start and ensure it is deferred.
class DidStartNavigationObserver : public content::WebContentsObserver {
 public:
  explicit DidStartNavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void ExpectNavigationDeferredAfterStart() {
    run_loop_.Run();
    EXPECT_TRUE(handle_);
    EXPECT_TRUE(handle_->IsDeferredForTesting());
  }

 private:
  // WebContentsObserver implementation:
  void DidStartNavigation(content::NavigationHandle* handle) override {
    handle_ = handle;
    run_loop_.Quit();
  }

  content::NavigationHandle* handle_ = nullptr;
  base::RunLoop run_loop_;
};

}  // namespace

using UserScriptListenerTest = ExtensionBrowserTest;

// Test that navigations block while waiting for content scripts to load.
IN_PROC_BROWSER_TEST_F(UserScriptListenerTest,
                       NavigationWaitsForContentScriptsToLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestingProfile profile;
  ExtensionsBrowserClient::Get()
      ->GetUserScriptListener()
      ->SetUserScriptsNotReadyForTesting(&profile);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  DidStartNavigationObserver start_observer(web_contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/echo"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  start_observer.ExpectNavigationDeferredAfterStart();

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(&profile),
      content::NotificationService::NoDetails());

  nav_observer.Wait();
}

}  // namespace extensions
