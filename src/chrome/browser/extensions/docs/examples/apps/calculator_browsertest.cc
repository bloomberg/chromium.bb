// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"

class CalculatorBrowserTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(CalculatorBrowserTest, Model) {
  base::FilePath test_file;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.DirName().DirName()
      .AppendASCII("common").AppendASCII("extensions").AppendASCII("docs")
      .AppendASCII("examples").AppendASCII("apps").AppendASCII("calculator")
      .AppendASCII("tests").AppendASCII("automatic.html");

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  bool success;
  bool executed = content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(window.runTests().success)",
      &success);

  ASSERT_TRUE(executed);
  ASSERT_TRUE(success);
}
