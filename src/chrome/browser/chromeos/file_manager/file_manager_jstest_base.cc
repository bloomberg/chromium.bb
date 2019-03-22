// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"

FileManagerJsTestBase::FileManagerJsTestBase(const base::FilePath& base_path)
    : base_path_(base_path) {}

void FileManagerJsTestBase::RunTest(const base::FilePath& file) {
  base::FilePath root_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  base::FilePath full_path = root_path.Append(base_path_).Append(file);
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::PathExists(full_path)) << full_path.value();
  }
  RunTestImpl(net::FilePathToFileURL(full_path));
}

void FileManagerJsTestBase::RunGeneratedTest(const std::string& file) {
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &path));
  path = path.AppendASCII("gen");

  // Serve the generated html file from out/gen. It references files from
  // DIR_SOURCE_ROOT, so serve from there as well. An alternative would be to
  // copy the js files as a build step and serve file:// URLs, but the embedded
  // test server gives better output for troubleshooting errors.
  embedded_test_server()->ServeFilesFromDirectory(path.Append(base_path_));
  embedded_test_server()->ServeFilesFromSourceDirectory(base::FilePath());

  ASSERT_TRUE(embedded_test_server()->Start());
  RunTestImpl(embedded_test_server()->GetURL(file));
}

void FileManagerJsTestBase::RunTestImpl(const GURL& url) {
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, {}));
}
