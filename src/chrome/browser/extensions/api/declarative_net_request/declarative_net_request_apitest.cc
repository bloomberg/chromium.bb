// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

namespace {

using ContextType = extensions::ExtensionApiTest::ContextType;
using extensions::ScopedCurrentChannel;

class DeclarativeNetRequestApiTest : public extensions::ExtensionApiTest {
 public:
  DeclarativeNetRequestApiTest() = default;
  explicit DeclarativeNetRequestApiTest(ContextType context_type)
      : ExtensionApiTest(context_type) {}
  ~DeclarativeNetRequestApiTest() override = default;
  DeclarativeNetRequestApiTest(const DeclarativeNetRequestApiTest&) = delete;
  DeclarativeNetRequestApiTest& operator=(const DeclarativeNetRequestApiTest&) =
      delete;

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
};

class DeclarativeNetRequestLazyApiTest
    : public DeclarativeNetRequestApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  DeclarativeNetRequestLazyApiTest()
      : DeclarativeNetRequestApiTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         DeclarativeNetRequestLazyApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(EventPage,
                         DeclarativeNetRequestLazyApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         DeclarativeNetRequestLazyApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyApiTest, DynamicRules) {
  ASSERT_TRUE(RunExtensionTest("dynamic_rules")) << message_;
}

// Flaky on ASAN/MSAN: https://crbug.com/1167168
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_DynamicRulesLimits DISABLED_DynamicRulesLimits
#else
#define MAYBE_DynamicRulesLimits DynamicRulesLimits
#endif
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyApiTest,
                       MAYBE_DynamicRulesLimits) {
  ASSERT_TRUE(RunExtensionTest("dynamic_rules_limits")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyApiTest, OnRulesMatchedDebug) {
  ASSERT_TRUE(RunExtensionTest("on_rules_matched_debug")) << message_;
}

// This test uses webRequest/webRequestBlocking, so it's not currently
// supported for service workers.
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, ModifyHeaders) {
  ASSERT_TRUE(RunExtensionTest("modify_headers")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyApiTest, GetMatchedRules) {
  ASSERT_TRUE(RunExtensionTest("get_matched_rules")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyApiTest, IsRegexSupported) {
  ASSERT_TRUE(RunExtensionTest("is_regex_supported")) << message_;
}

class DeclarativeNetRequestApiFencedFrameTest
    : public DeclarativeNetRequestApiTest,
      public testing::WithParamInterface<bool /* shadow_dom_fenced_frame */> {
 protected:
  DeclarativeNetRequestApiFencedFrameTest()
      : DeclarativeNetRequestApiTest(ContextType::kPersistentBackground) {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames,
        {{"implementation_type", GetParam() ? "shadow_dom" : "mparch"}});
  }

  ~DeclarativeNetRequestApiFencedFrameTest() override = default;

  void SetUpOnMainThread() override {
    DeclarativeNetRequestApiTest::SetUpOnMainThread();
    // Give the test knowledge if we are in MPArch or not. As the frame
    // ids will be different because of ShadowDOM.
    if (!GetParam()) {
      SetCustomArg("MPArch");
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestApiFencedFrameTest, Load) {
  ASSERT_TRUE(RunExtensionTest("fenced_frames")) << message_;
}

INSTANTIATE_TEST_SUITE_P(DeclarativeNetRequestApiFencedFrameTest,
                         DeclarativeNetRequestApiFencedFrameTest,
                         testing::Bool());

}  // namespace
