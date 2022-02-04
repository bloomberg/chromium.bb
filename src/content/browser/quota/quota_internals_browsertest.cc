// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/browser/quota/quota_internals_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kQuotaInternalsUrl[] = "chrome://quota-internals-2/";

}  // namespace

class QuotaInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  QuotaInternalsWebUiBrowserTest() = default;

 protected:
  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  bool ExecJsInWebUI(const std::string& script) {
    return ExecJs(shell()->web_contents()->GetMainFrame(), script,
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */);
  }
};

// Ensures that the page is loaded correctly.
IN_PROC_BROWSER_TEST_F(QuotaInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kQuotaInternalsUrl)));
  EXPECT_TRUE(
      ExecJsInWebUI("document.body.innerHTML.search('Total Space') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.body.innerHTML.search('Available Space') >= 0;"));
  EXPECT_TRUE(ExecJsInWebUI("document.getElementById('total-space') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('available-space') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.body.innerHTML.search('Errors on Getting Usage "
                    "and Quota') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.body.innerHTML.search('Evicted Buckets') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.body.innerHTML.search('Eviction Rounds') >= 0;"));
  EXPECT_TRUE(ExecJsInWebUI(
      "document.body.innerHTML.search('Skipped Eviction Rounds') >= 0;"));
  EXPECT_TRUE(ExecJsInWebUI(
      "document.getElementById('errors-on-getting-usage-and-quota') >= 0;"));
  EXPECT_TRUE(ExecJsInWebUI(
      "document.getElementById('skipped-eviction-rounds') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('eviction-rounds') >= 0;"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('evicted-buckets') >= 0;"));
}

}  // namespace content
