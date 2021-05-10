// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the --load-and-launch-app switch.
// The two cases are when chrome is running and another process uses the switch
// and when chrome is started from scratch.

#include "apps/switches.h"
#include "base/base_switches.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "sandbox/policy/switches.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

using extensions::PlatformAppBrowserTest;

namespace apps {

namespace {

constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

// Lacros doesn't support launching with chrome already running. See the header
// comment for InProcessBrowserTest::GetCommandLineForRelaunch().
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

const char* kSwitchesToCopy[] = {
    sandbox::policy::switches::kNoSandbox,
    switches::kUserDataDir,
#if defined(USE_OZONE)
    // Keep the kOzonePlatform switch that the Ozone must use.
    switches::kOzonePlatform,
#endif
    // Some tests use custom cmdline that doesn't hold switches from previous
    // cmdline. Only a couple of switches are copied. That can result in
    // incorrect initialization of a process. For example, the work that we do
    // to have use_x11 && use_ozone, requires UseOzonePlatform feature flag to
    // be passed to all the process to ensure correct path is chosen.
    // TODO(https://crbug.com/1096425): update this comment once USE_X11 goes
    // away.
    switches::kEnableFeatures,
    switches::kDisableFeatures,
};

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465
#if defined(OS_MAC)
#define MAYBE_LoadAndLaunchAppChromeRunning \
        DISABLED_LoadAndLaunchAppChromeRunning
#else
#define MAYBE_LoadAndLaunchAppChromeRunning LoadAndLaunchAppChromeRunning
#endif

// Case where Chrome is already running.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppChromeRunning) {
  ExtensionTestMessageListener launched_listener("Launched", false);

  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  base::CommandLine new_cmdline(cmdline.GetProgram());
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchesToCopy,
                               base::size(kSwitchesToCopy));

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("minimal");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());

  new_cmdline.AppendSwitch(switches::kLaunchAsBrowser);
  base::Process process =
      base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest());
  ASSERT_TRUE(process.IsValid());

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  ASSERT_EQ(0, exit_code);
}

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465.
#if defined(OS_MAC)
#define MAYBE_LoadAndLaunchAppWithFile DISABLED_LoadAndLaunchAppWithFile
#else
#define MAYBE_LoadAndLaunchAppWithFile LoadAndLaunchAppWithFile
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppWithFile) {
  ExtensionTestMessageListener launched_listener("Launched", false);

  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  base::CommandLine new_cmdline(cmdline.GetProgram());
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchesToCopy,
                               base::size(kSwitchesToCopy));

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("load_and_launch_file");

  base::FilePath test_file_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("launch_files")
      .AppendASCII("test.txt");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());
  new_cmdline.AppendSwitch(switches::kLaunchAsBrowser);
  new_cmdline.AppendArgPath(test_file_path);

  base::Process process =
      base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest());
  ASSERT_TRUE(process.IsValid());

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  ASSERT_EQ(0, exit_code);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TestFixture that appends --load-and-launch-app with an app before calling
// BrowserMain.
class LoadAndLaunchPlatformAppBrowserTest : public PlatformAppBrowserTest {
 public:
  LoadAndLaunchPlatformAppBrowserTest(
      const LoadAndLaunchPlatformAppBrowserTest&) = delete;
  LoadAndLaunchPlatformAppBrowserTest& operator=(
      const LoadAndLaunchPlatformAppBrowserTest&) = delete;

 protected:
  LoadAndLaunchPlatformAppBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    base::FilePath app_path =
        test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp, app_path.value());
  }

  void LoadAndLaunchApp() {
    ExtensionTestMessageListener launched_listener("Launched", false);
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    // Start an actual browser because we can't shut down with just an app
    // window.
    CreateBrowser(ProfileManager::GetActiveUserProfile());
  }
};

// TestFixture that appends --load-and-launch-app with an extension before
// calling BrowserMain.
class LoadAndLaunchExtensionBrowserTest : public PlatformAppBrowserTest {
 public:
  LoadAndLaunchExtensionBrowserTest(const LoadAndLaunchExtensionBrowserTest&) =
      delete;
  LoadAndLaunchExtensionBrowserTest& operator=(
      const LoadAndLaunchExtensionBrowserTest&) = delete;

 protected:
  LoadAndLaunchExtensionBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    base::FilePath app_path = test_data_dir_.AppendASCII("good")
                                  .AppendASCII("Extensions")
                                  .AppendASCII(kTestExtensionId)
                                  .AppendASCII("1.0.0.0");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp, app_path.value());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();

    // Skip showing the error message box to avoid freezing the main thread.
    chrome::internal::g_should_skip_message_box_for_test = true;
  }
};

// Case where Chrome is not running.
IN_PROC_BROWSER_TEST_F(LoadAndLaunchPlatformAppBrowserTest,
                       LoadAndLaunchAppChromeNotRunning) {
  LoadAndLaunchApp();
}

// TODO(https://crbug.com/988160): Test is flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_LoadAndLaunchExtension DISABLED_LoadAndLaunchExtension
#else
#define MAYBE_LoadAndLaunchExtension LoadAndLaunchExtension
#endif
IN_PROC_BROWSER_TEST_F(LoadAndLaunchExtensionBrowserTest,
                       MAYBE_LoadAndLaunchExtension) {
  const std::vector<base::string16>* errors =
      extensions::LoadErrorReporter::GetInstance()->GetErrors();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The error is skipped on official builds.
  EXPECT_TRUE(errors->empty());
#else
  // Expect |extension_instead_of_app_error|.
  EXPECT_EQ(1u, errors->size());
  EXPECT_NE(base::string16::npos,
            errors->at(0).find(base::ASCIIToUTF16(
                "App loading flags cannot be used to load extensions")));
#endif

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_EQ(nullptr,
            registry->GetExtensionById(
                kTestExtensionId, extensions::ExtensionRegistry::EVERYTHING));
}

}  // namespace
}  // namespace apps
