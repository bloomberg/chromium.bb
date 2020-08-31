// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

class ExperimentalApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

class PermissionsApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(PermissionsApiTest, PermissionsFail) {
  ASSERT_TRUE(RunExtensionTest("permissions/disabled")) << message_;

  // Since the experimental APIs require a flag, this will fail even though
  // it's enabled.
  // TODO(erikkay) This test is currently broken because LoadExtension in
  // ExtensionBrowserTest doesn't actually fail, it just times out.  To fix this
  // I'll need to add an EXTENSION_LOAD_ERROR notification, which is probably
  // too much for the branch.  I'll enable this on trunk later.
  // ASSERT_FALSE(RunExtensionTest("permissions/enabled"))) << message_;
}

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, PermissionsSucceed) {
  ASSERT_TRUE(RunExtensionTest("permissions/enabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(PermissionsApiTest, ExperimentalPermissionsFail) {
  // At the time this test is being created, there is no experimental
  // function that will not be graduating soon, and does not require a
  // tab id as an argument.  So, we need the tab permission to get
  // a tab id.
  ASSERT_TRUE(RunExtensionTest("permissions/experimental_disabled"))
      << message_;
}

// TODO(crbug/1065399): Flaky on ChromeOS and Linux non-dbg builds.
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(NDEBUG)
#define MAYBE_FaviconPermission DISABLED_FaviconPermission
#else
#define MAYBE_FaviconPermission FaviconPermission
#endif
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, MAYBE_FaviconPermission) {
  base::HistogramTester tester;
  ASSERT_TRUE(RunExtensionTest("permissions/favicon")) << message_;
  tester.ExpectBucketCount("Extensions.FaviconResourceRequested",
                           Manifest::TYPE_EXTENSION, 1);
}

// Test functions and APIs that are always allowed (even if you ask for no
// permissions).
// Flaky on MacOS (see crbug/1064929).
#if defined(OS_MACOSX)
#define MAYBE_AlwaysAllowed DISABLED_AlwaysAllowed
#else
#define MAYBE_AlwaysAllowed AlwaysAllowed
#endif
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, MAYBE_AlwaysAllowed) {
  ASSERT_TRUE(RunExtensionTest("permissions/always_allowed")) << message_;
}

// Tests that the optional permissions API works correctly.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsGranted) {
  // Mark all the tested APIs as granted to bypass the confirmation UI.
  APIPermissionSet apis;
  apis.insert(APIPermission::kBookmark);
  URLPatternSet explicit_hosts;
  AddPattern(&explicit_hosts, "http://*.c.com/*");

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  prefs->AddRuntimeGrantedPermissions(
      "kjmkgkdkpedkejedfhmfcenooemhbpbo",
      PermissionSet(std::move(apis), ManifestPermissionSet(),
                    std::move(explicit_hosts), URLPatternSet()));

  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional")) << message_;
}

// Tests that the optional permissions API works correctly.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsAutoConfirm) {
  // Rather than setting the granted permissions, set the UI autoconfirm flag
  // and run the same tests.
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional")) << message_;
}

// Test that denying the optional permissions confirmation dialog works.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsDeny) {
  // Mark the management permission as already granted since we auto reject
  // user prompts.
  APIPermissionSet apis;
  apis.insert(APIPermission::kManagement);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  prefs->AddRuntimeGrantedPermissions(
      "kjmkgkdkpedkejedfhmfcenooemhbpbo",
      PermissionSet(std::move(apis), ManifestPermissionSet(), URLPatternSet(),
                    URLPatternSet()));

  PermissionsRequestFunction::SetAutoConfirmForTests(false);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_deny")) << message_;
}

// Tests that the permissions.request function must be called from within a
// user gesture.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsGesture) {
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_gesture")) << message_;
}

// Tests that the user gesture is retained in the permissions.request function
// callback.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsRetainGesture) {
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_retain_gesture"))
      << message_;
}

// Test that optional permissions blocked by enterprise policy will be denied
// automatically.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       OptionalPermissionsPolicyBlocked) {
  // Set enterprise policy to block some API permissions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddBlockedPermission("*", "management");
  }
  // Set auto confirm UI flag.
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  EXPECT_TRUE(RunExtensionTest("permissions/optional_policy_blocked"))
      << message_;
}

// Tests that an extension can't gain access to file: URLs without the checkbox
// entry in prefs. There shouldn't be a warning either.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsFileAccess) {
  // There shouldn't be a warning, so we shouldn't need to autoconfirm.
  PermissionsRequestFunction::SetAutoConfirmForTests(false);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());

  EXPECT_TRUE(
      RunExtensionTestNoFileAccess("permissions/file_access_no")) << message_;
  EXPECT_FALSE(prefs->AllowFileAccess("dgloelfbnddbdacakahpogklfdcccbib"));

  EXPECT_TRUE(RunExtensionTest("permissions/file_access_yes")) << message_;
  // TODO(kalman): ugh, it would be nice to test this condition, but it seems
  // like there's somehow a race here where the prefs aren't updated in time
  // with the "allow file access" bit, so we'll just have to trust that
  // RunExtensionTest (unlike RunExtensionTestNoFileAccess) does indeed
  // not set the allow file access bit. Otherwise this test doesn't mean
  // a whole lot (i.e. file access works - but it'd better not be the case
  // that the extension actually has file access, since that'd be the bug
  // that this is supposed to be testing).
  // EXPECT_TRUE(prefs->AllowFileAccess("hlonmbgfjccgolnaboonlakjckinmhmd"));
}

// Tests loading of files or directory listings when an extension has file
// access.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, FileLoad) {
  base::ScopedTempDir temp_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath empty_file = temp_dir.GetPath().AppendASCII("empty.html");
    base::FilePath original_empty_file = ui_test_utils::GetTestFilePath(
        base::FilePath(), base::FilePath().AppendASCII("empty.html"));

    EXPECT_TRUE(base::PathExists(original_empty_file));
    EXPECT_TRUE(base::CopyFile(original_empty_file, empty_file));
  }
  EXPECT_TRUE(RunExtensionTestWithFlagsAndArg(
      "permissions/file_load", temp_dir.GetPath().MaybeAsASCII().c_str(),
      kFlagEnableFileAccess, kFlagNone))
      << message_;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.Delete());
  }
}

// Test requesting, querying, and removing host permissions for host
// permissions that are a subset of the optional permissions.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, HostSubsets) {
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  EXPECT_TRUE(RunExtensionTest("permissions/host_subsets")) << message_;
}

// Tests that requesting an optional permission from a background page, with
// another window open, grants the permission and updates the bindings
// (chrome.whatever, in this case chrome.alarms). Regression test for
// crbug.com/435141, see details there for trickiness.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsUpdatesBindings) {
  ASSERT_TRUE(RunExtensionTest("permissions/optional_updates_bindings"))
      << message_;
}

}  // namespace extensions
