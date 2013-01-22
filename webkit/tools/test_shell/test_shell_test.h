// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base test class used by all test shell tests.  Provides boiler plate
 * code to create and destroy a new test shell for each gTest test.
 */

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__

#include <string>

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "webkit/tools/test_shell/test_shell.h"

class TestShellTest : public testing::Test {
 protected:
  // Returns the path "test_case_path/test_case".
  GURL GetTestURL(const FilePath& test_case_path,
                  const std::string& test_case);

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Don't refactor away; some unittests override this!
  virtual void CreateEmptyWindow();

 protected:
  // Location of SOURCE_ROOT/webkit/data/
  FilePath data_dir_;

  TestShell* test_shell_;
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__
