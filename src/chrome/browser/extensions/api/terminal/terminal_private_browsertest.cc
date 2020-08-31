// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class TerminalPrivateBrowserTest : public InProcessBrowserTest {
 public:
  TerminalPrivateBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTerminalSystemApp);
  }

 protected:
  void ExpectJsResult(const std::string& script, const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::EvalJsResult eval_result =
        EvalJs(web_contents, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               /*world_id=*/1);
    EXPECT_EQ(eval_result.value.GetString(), expected);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(TerminalPrivateBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenTerminalProcessChecks) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://terminal/html/terminal.html"));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openVmshellProcess([], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })}))";

  // 'vmshell not allowed' when crostini is not allowed.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_allowed(false);
  ExpectJsResult(script, "vmshell not allowed");

  // 'Error starting crostini for terminal: 22' when crostini is allowed.
  // This is the specific error we expect to see in a test environment
  // (LOAD_COMPONENT_FAILED), but the point of the test is to see that we get
  // past the vmshell allowed checks.
  crostini_features.set_allowed(true);
  ExpectJsResult(script, "Waiting for component update dialog response");

  // openTerminalProcess not defined.
  ExpectJsResult("typeof chrome.terminalPrivate.openTerminalProcess",
                 "undefined");
}

}  // namespace extensions
