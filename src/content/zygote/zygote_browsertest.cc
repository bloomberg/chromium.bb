// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"
#include "services/service_manager/zygote/host/zygote_communication_linux.h"
#include "services/service_manager/zygote/host/zygote_host_impl_linux.h"
#endif

namespace content {

class LinuxZygoteBrowserTest : public ContentBrowserTest {
 public:
  LinuxZygoteBrowserTest() = default;
  ~LinuxZygoteBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxZygoteBrowserTest);
};

// https://crbug.com/638303
IN_PROC_BROWSER_TEST_F(LinuxZygoteBrowserTest, GetLocalTimeHasTimeZone) {
  const char kTestCommand[] =
      "window.domAutomationController.send(new Date().toString());";

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,start page")));
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(shell(), kTestCommand, &result));
  std::vector<std::string> parts = base::SplitString(
      result, "()", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(3U, parts.size());
  EXPECT_FALSE(parts[0].empty());
  EXPECT_FALSE(parts[1].empty());
  EXPECT_TRUE(parts[2].empty());
}

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
IN_PROC_BROWSER_TEST_F(LinuxZygoteBrowserTest, ZygoteSandboxes) {
  // We need zygotes and the standard sandbox config to run this test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoZygote) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoSandbox)) {
    return;
  }

  // Sanity check the sandbox flags we expect to be everywhere.
  const int flags = service_manager::GetGenericZygote()->GetSandboxStatus();
  constexpr int kExpectedFlags = service_manager::SandboxLinux::kPIDNS |
                                 service_manager::SandboxLinux::kNetNS |
                                 service_manager::SandboxLinux::kUserNS;
  EXPECT_EQ(kExpectedFlags, flags & kExpectedFlags);

  EXPECT_EQ(service_manager::GetUnsandboxedZygote()->GetSandboxStatus(), 0);
}
#endif

class LinuxZygoteDisabledBrowserTest : public ContentBrowserTest {
 public:
  LinuxZygoteDisabledBrowserTest() = default;
  ~LinuxZygoteDisabledBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoZygote);
    command_line->AppendSwitch(service_manager::switches::kNoSandbox);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxZygoteDisabledBrowserTest);
};

// https://crbug.com/712779
#if !defined(THREAD_SANITIZER)
// Test that the renderer doesn't crash during launch if zygote is disabled.
IN_PROC_BROWSER_TEST_F(LinuxZygoteDisabledBrowserTest,
                       NoCrashWhenZygoteDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,start page")));
}
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
IN_PROC_BROWSER_TEST_F(LinuxZygoteDisabledBrowserTest,
                       NoZygoteWhenZygoteDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,start page")));

  EXPECT_FALSE(service_manager::ZygoteHostImpl::GetInstance()->HasZygote());
}
#endif

}  // namespace content
