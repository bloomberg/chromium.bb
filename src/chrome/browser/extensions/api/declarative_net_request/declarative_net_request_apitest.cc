// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {

class DeclarativeNetRequestAPItest : public extensions::ExtensionApiTest {
 public:
  DeclarativeNetRequestAPItest() {}

 protected:
  // ExtensionApiTest override.
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir =
        test_data_dir_.AppendASCII("declarative_net_request");

    // Copy the |test_data_dir| to a temporary location. We do this to ensure
    // that the temporary kMetadata folder created as a result of loading the
    // extension is not written to the src directory and is automatically
    // removed.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::CopyDirectory(test_data_dir, temp_dir_.GetPath(), true /*recursive*/);

    // Override the path used for loading the extension.
    test_data_dir_ = temp_dir_.GetPath().AppendASCII("declarative_net_request");
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestAPItest);
};

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestAPItest, DynamicRules) {
  ASSERT_TRUE(RunExtensionTest("dynamic_rules")) << message_;
}

// TODO(crbug.com/1029233) Restore this test. This is disabled due to
// flakiness.
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestAPItest,
                       DISABLED_OnRulesMatchedDebug) {
  ASSERT_TRUE(RunExtensionTest("on_rules_matched_debug")) << message_;
}

// TODO(crbug.com/1070344): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestAPItest, DISABLED_GetMatchedRules) {
  ASSERT_TRUE(RunExtensionTest("get_matched_rules")) << message_;
}

}  // namespace
