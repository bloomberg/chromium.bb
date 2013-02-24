// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_test.h"

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

GURL TestShellTest::GetTestURL(const base::FilePath& test_case_path,
                               const std::string& test_case) {
  return net::FilePathToFileURL(test_case_path.AppendASCII(test_case));
}

void TestShellTest::SetUp() {
  // Make a test shell for use by the test.
  CreateEmptyWindow();
  test_shell_->Show(WebKit::WebNavigationPolicyNewWindow);
  webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
      "TestShell/0.0.0.0"), false);

  // Point data_dir_ to the root of the test case dir
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir_));
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("webkit"));
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("data"));
  ASSERT_TRUE(file_util::PathExists(data_dir_));
}

void TestShellTest::TearDown() {
  // Loading a blank url clears the memory in the current page.
  test_shell_->LoadURL(GURL("about:blank"));
  test_shell_->DestroyWindow(test_shell_->mainWnd());

  // Flush the MessageLoop of any residual tasks.
  MessageLoop::current()->RunUntilIdle();
}

void TestShellTest::CreateEmptyWindow() {
  TestShell::CreateNewWindow(GURL("about:blank"), &test_shell_);
}
