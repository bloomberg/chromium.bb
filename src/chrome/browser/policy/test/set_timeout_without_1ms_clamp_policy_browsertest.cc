// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestSetTimeoutWithout1MsClamp : public PolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    SetPolicy(&policies, policy::key::kSetTimeoutWithout1MsClampEnabled,
              base::Value(false));
    provider_.UpdateChromePolicy(policies);
  }
};

constexpr char kSetTimeout0Script[] =
    R"({
      const last = performance.now();
      let elapsed;
      function setTimeoutCallback() {
        const current = performance.now();
        elapsed = current - last;
        window.domAutomationController.send(elapsed > 1);
      }
      setTimeout(setTimeoutCallback, 0);
    })";

IN_PROC_BROWSER_TEST_F(PolicyTestSetTimeoutWithout1MsClamp, DisablePolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      policy::policy_prefs::kSetTimeoutWithout1MsClampEnabled));

  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kSetTimeout0Script);
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);
}

}  // namespace policy
